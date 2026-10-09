#pragma once

#include <stddef.h>
#include <stdint.h>
#include <queue>

static constexpr size_t GPIO_FIFO_DEPTH = 32;

enum class MODE : uint8_t {
    RELEASE,
    WAIT,
    SET0,
    SET1,
    SHIFT_IN,
    SHIFT_OUT,
    CLK_GEN,
    BAUD_GEN
};

class GPIO_SM {
private:
    struct Registers {
        MODE mode = MODE::RELEASE;

        uint32_t n = 0;
        uint32_t counter = 0;

        // Sticky edge flags; clear through clear_edge_flags().
        bool flag_posedge = false;
        bool flag_negedge = false;
        bool flag_ctr_zero = false;

        bool pin_out = false;
        bool pin_oe = false;
        bool pin_in_prev = false;

        uint8_t pckt_len = 32;

        // Receive shifter.
        uint8_t rx_shift = 0;
        uint8_t rx_bits_in_byte = 0;
        uint8_t rx_packet_bits = 0;

        // Transmit shifter.
        uint8_t tx_shift = 0;
        uint8_t tx_bits_in_byte = 0;
        uint8_t tx_packet_bits_left = 0;

        bool fifo_overflow = false;
    };

    Registers curr_{};
    Registers next_{};

    std::queue<uint8_t> fifo_;

    // Asynchronous pad input and two-stage synchronizer.
    bool pin_in_async_ = false;
    bool sync_ff1_ = false;
    bool sync_ff2_ = false;

    bool open_drain_ = false;
    bool idle_level_ = false;

    // Edge events calculated by comb().
    bool rise_event_ = false;
    bool fall_event_ = false;

    // Deferred FIFO operations.
    bool fifo_push_pending_ = false;
    bool fifo_pop_pending_ = false;
    uint8_t fifo_push_data_ = 0;

    // Software clear requests.
    bool clear_posedge_pending_ = false;
    bool clear_negedge_pending_ = false;

    void schedule_fifo_push(uint8_t value) {
        if (fifo_.size() < GPIO_FIFO_DEPTH) {
            fifo_push_pending_ = true;
            fifo_push_data_ = value;
        } else {
            next_.fifo_overflow = true;
        }
    }

public:
    GPIO_SM() = default;

    // Configure the engine. Existing FIFO entries are preserved.
    void config(MODE mode, uint32_t n, uint8_t pckt_len = 32) {
        curr_.mode = mode;
        curr_.n = n;
        curr_.counter = n;

        curr_.pckt_len =
            (pckt_len == 0 || pckt_len > 32) ? 32 : pckt_len;

        curr_.rx_shift = 0;
        curr_.rx_bits_in_byte = 0;
        curr_.rx_packet_bits = 0;

        curr_.tx_shift = 0;
        curr_.tx_bits_in_byte = 0;
        curr_.tx_packet_bits_left = 0;

        comb();
    }

    // Set the raw asynchronous pad input.
    void set_pin_in(bool value) {
        pin_in_async_ = value;
    }

    bool get_pin_in_async() const {
        return pin_in_async_;
    }

    // Synchronized input after two clock stages.
    bool get_pin_in() const {
        return sync_ff2_;
    }

    // Reflect the synchronized level without another status-register delay.
    bool get_flag_level() const {
        return sync_ff2_;
    }

    bool get_pin_out() const {
        return curr_.pin_out;
    }

    // True means the output driver is enabled.
    bool get_pin_oe() const {
        return curr_.pin_oe;
    }

    // Sticky flags: remain set until explicitly cleared.
    bool get_flag_posedge() const {
        return curr_.flag_posedge;
    }

    bool get_flag_negedge() const {
        return curr_.flag_negedge;
    }

    // Counter terminal-count pulse.
    bool get_flag_ctr_zero() const {
        return curr_.flag_ctr_zero;
    }

    bool get_fifo_overflow() const {
        return curr_.fifo_overflow;
    }

    // Write-one-to-clear style behavior. If a new edge is detected
    // on the clearing edge, the new event remains set.
    void clear_edge_flags(bool clear_posedge, bool clear_negedge) {
        clear_posedge_pending_ |= clear_posedge;
        clear_negedge_pending_ |= clear_negedge;
        comb();
    }

    void clear_fifo_overflow() {
        curr_.fifo_overflow = false;
        next_.fifo_overflow = false;
        comb();
    }

    // Logical high releases the pin in open-drain mode.
    void set_open_drain(bool enabled) {
        open_drain_ = enabled;
        comb();
    }

    void set_idle_level(bool high) {
        idle_level_ = high;
        comb();
    }

    // FIFO_BUFF access: one byte per operation.
    // Do not call between comb() and seq() for the same clock edge.
    bool set_fifo(uint8_t value) {
        if (fifo_.size() >= GPIO_FIFO_DEPTH) {
            curr_.fifo_overflow = true;
            next_.fifo_overflow = true;
            comb();
            return false;
        }

        fifo_.push(value);
        comb();
        return true;
    }

    bool try_get_fifo(uint8_t& value) {
        if (fifo_.empty()) {
            return false;
        }

        value = fifo_.front();
        fifo_.pop();
        comb();
        return true;
    }

    // Returns zero if empty; use try_get_fifo() to distinguish empty from zero.
    uint8_t get_fifo() {
        uint8_t value = 0;
        (void)try_get_fifo(value);
        return value;
    }

    size_t fifo_size() const {
        return fifo_.size();
    }

    bool fifo_empty() const {
        return fifo_.empty();
    }

    bool fifo_full() const {
        return fifo_.size() >= GPIO_FIFO_DEPTH;
    }

    void clear_fifo() {
        while (!fifo_.empty()) {
            fifo_.pop();
        }

        comb();
    }

    // Calculate next state without modifying current state or FIFO contents.
    // May be called multiple times before a clock edge.
    void comb() {
        next_ = curr_;

        fifo_push_pending_ = false;
        fifo_pop_pending_ = false;
        fifo_push_data_ = 0;

        const bool pin_sample = sync_ff2_;
        const bool terminal = (curr_.counter == 0);

        // Generate edge events from the synchronized input.
        rise_event_ = pin_sample && !curr_.pin_in_prev;
        fall_event_ = !pin_sample && curr_.pin_in_prev;

        // Sticky status flags.
        next_.flag_posedge = curr_.flag_posedge || rise_event_;
        next_.flag_negedge = curr_.flag_negedge || fall_event_;

        next_.pin_in_prev = pin_sample;

        // Counter period is N+1 system-clock cycles.
        next_.flag_ctr_zero = terminal;
        next_.counter = terminal ? curr_.n : curr_.counter - 1u;

        switch (curr_.mode) {
            case MODE::RELEASE:
                next_.pin_oe = false;
                break;

            case MODE::WAIT:
                next_.pin_oe = false;
                break;

            case MODE::SET0:
                next_.pin_out = false;
                next_.pin_oe = true;
                break;

            case MODE::SET1:
                next_.pin_out = true;
                next_.pin_oe = true;
                break;

            case MODE::SHIFT_IN: {
                next_.pin_oe = false;

                if (terminal) {
                    const uint8_t sampled = static_cast<uint8_t>(
                        static_cast<uint8_t>(curr_.rx_shift << 1u) |
                        (pin_sample ? 1u : 0u)
                    );

                    const uint8_t byte_bits =
                        static_cast<uint8_t>(curr_.rx_bits_in_byte + 1u);

                    const uint8_t packet_bits =
                        static_cast<uint8_t>(curr_.rx_packet_bits + 1u);

                    const bool byte_complete = (byte_bits == 8u);
                    const bool packet_complete =
                        (packet_bits >= curr_.pckt_len);

                    if (byte_complete) {
                        schedule_fifo_push(sampled);
                        next_.rx_shift = 0;
                        next_.rx_bits_in_byte = 0;
                    } else {
                        next_.rx_shift = sampled;
                        next_.rx_bits_in_byte = byte_bits;
                    }

                    if (packet_complete) {
                        // Left-align a partial final byte; pad low bits with zero.
                        if (!byte_complete) {
                            const uint8_t padded = static_cast<uint8_t>(
                                sampled << (8u - byte_bits)
                            );

                            schedule_fifo_push(padded);
                        }

                        next_.rx_shift = 0;
                        next_.rx_bits_in_byte = 0;
                        next_.rx_packet_bits = 0;
                    } else {
                        next_.rx_packet_bits = packet_bits;
                    }
                }

                break;
            }

            case MODE::SHIFT_OUT: {
                next_.pin_oe = true;

                if (terminal) {
                    const uint8_t packet_left =
                        (curr_.tx_packet_bits_left == 0)
                            ? curr_.pckt_len
                            : curr_.tx_packet_bits_left;

                    bool have_bit = false;
                    bool bit = idle_level_;

                    if (curr_.tx_bits_in_byte != 0u) {
                        // Emit the next bit from the current byte.
                        bit = (curr_.tx_shift & 0x80u) != 0u;

                        next_.tx_shift =
                            static_cast<uint8_t>(curr_.tx_shift << 1u);

                        next_.tx_bits_in_byte =
                            static_cast<uint8_t>(
                                curr_.tx_bits_in_byte - 1u
                            );

                        have_bit = true;
                    } else if (!fifo_.empty()) {
                        // Peek now; pop is committed by seq().
                        const uint8_t byte = fifo_.front();

                        fifo_pop_pending_ = true;

                        bit = (byte & 0x80u) != 0u;

                        next_.tx_shift =
                            static_cast<uint8_t>(byte << 1u);

                        next_.tx_bits_in_byte = 7;
                        have_bit = true;
                    }

                    if (have_bit) {
                        next_.pin_out = bit;

                        next_.tx_packet_bits_left =
                            static_cast<uint8_t>(packet_left - 1u);

                        if (next_.tx_packet_bits_left == 0u) {
                            // Discard unused bits at the end of this packet.
                            next_.tx_shift = 0;
                            next_.tx_bits_in_byte = 0;
                        }
                    } else {
                        // Underflow stalls an incomplete packet.
                        next_.pin_out =
                            (curr_.tx_packet_bits_left == 0)
                                ? idle_level_
                                : curr_.pin_out;

                        next_.tx_packet_bits_left =
                            curr_.tx_packet_bits_left;

                        next_.tx_shift = curr_.tx_shift;
                        next_.tx_bits_in_byte =
                            curr_.tx_bits_in_byte;
                    }
                }

                break;
            }

            case MODE::CLK_GEN:
                next_.pin_oe = true;

                if (terminal) {
                    next_.pin_out = !curr_.pin_out;
                }

                break;

            case MODE::BAUD_GEN:
                next_.pin_oe = true;
                next_.pin_out = terminal;
                break;
        }

        // Apply the electrical output policy.
        if (open_drain_ && next_.pin_out) {
            next_.pin_oe = false;
        }
    }

    // Commit next state, pending FIFO operations, and synchronizer stages.
    void seq() {
        if (fifo_pop_pending_ && !fifo_.empty()) {
            fifo_.pop();
        }

        bool overflow_on_commit = false;

        if (fifo_push_pending_) {
            if (fifo_.size() < GPIO_FIFO_DEPTH) {
                fifo_.push(fifo_push_data_);
            } else {
                overflow_on_commit = true;
            }
        }

        curr_ = next_;

        // Clear old events, but preserve an event detected on this edge.
        if (clear_posedge_pending_) {
            curr_.flag_posedge = rise_event_;
        }

        if (clear_negedge_pending_) {
            curr_.flag_negedge = fall_event_;
        }

        clear_posedge_pending_ = false;
        clear_negedge_pending_ = false;

        if (overflow_on_commit) {
            curr_.fifo_overflow = true;
        }

        // Simultaneous FF updates use the old stage-1 value.
        const bool old_ff1 = sync_ff1_;
        sync_ff2_ = old_ff1;
        sync_ff1_ = pin_in_async_;

        fifo_push_pending_ = false;
        fifo_pop_pending_ = false;
        fifo_push_data_ = 0;
    }

    // One rising edge followed by next-state recalculation.
    void posedge() {
        seq();
        comb();
    }
};
