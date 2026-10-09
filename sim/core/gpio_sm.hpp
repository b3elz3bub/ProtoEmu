#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>

static constexpr size_t FIFO_SIZE = 32;

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

        // 1-cycle instantaneous event flags
        bool flag_posedge = false;
        bool flag_negedge = false;
        bool flag_level = false;
        bool flag_ctr_zero = false;

        // Synchronized input history
        bool pin_in_sync1 = false;
        bool pin_in_sync2 = false;
        bool pin_in_prev = false;

        // Output states
        bool pin_out = false;
        bool pin_oe = false;

        // Serial transfer state (pckt_len ranges from 1 to 8 bits)
        uint8_t osr = 0;
        uint8_t bit_cnt = 0;
        uint8_t pckt_len = 8;

        // Fault insertion registers
        bool fault_en = false;
        bool skew_en = false;
        uint8_t skew_cnt = 0;
        uint8_t skew_counter = 0;
        bool skew_pending = false;
        bool skew_target = false;

        bool bit_flip = false;
        bool glitch_en = false;
        bool pin_out_actual = false;

        // Timing Monitor
        bool tmon_en = false;
        bool tmon_trig = false;
        uint16_t tmon_ctr = 0;
    };

    Registers curr{};
    Registers next{};

    std::queue<uint8_t> fifo;

    // Deferred FIFO flags to keep comb() pure
    bool fifo_push_pending = false;
    bool fifo_pop_pending = false;
    uint8_t fifo_push_data = 0;

public:
    bool pin_in = false;

    GPIO_SM() = default;

    // Configure engine. pckt_len clamped to 1..8 bits per word.
    void config(MODE mode, uint32_t n, uint8_t pckt_len = 8) {
        curr.mode = mode;
        curr.n = n;
        curr.counter = n;
        curr.pckt_len = (pckt_len == 0 || pckt_len > 8) ? 8 : pckt_len;
        curr.bit_cnt = 0;
        curr.osr = 0;
        comb();
    }

    uint8_t get_fifo() {
        if (fifo.empty()) return 0;
        uint8_t val = fifo.front();
        fifo.pop();
        return val;
    }

    void set_fifo(uint8_t val) {
        if (fifo.size() < FIFO_SIZE) fifo.push(val);
    }

    void set_pin_in(bool pin_in_val) {
        this->pin_in = pin_in_val;
        comb();
    }

    bool get_flag_posedge() const { return curr.flag_posedge; }
    bool get_flag_negedge() const { return curr.flag_negedge; }
    bool get_flag_ctr_zero() const { return curr.flag_ctr_zero; }
    bool get_flag_level() const { return curr.flag_level; }
    bool get_pin_out() const { return curr.pin_out; }

    // Fault injection getters/setters
    bool get_fault_en() const { return curr.fault_en; }
    void set_fault_en(bool en) { curr.fault_en = en; }

    bool get_skew_en() const { return curr.skew_en; }
    void set_skew_en(bool en) { curr.skew_en = en; }

    uint8_t get_skew_cnt() const { return curr.skew_cnt; }
    void set_skew_cnt(uint8_t cnt) { curr.skew_cnt = cnt; }

    bool get_bit_flip() const { return curr.bit_flip; }
    void set_bit_flip(bool en) { curr.bit_flip = en; }

    bool get_glitch_en() const { return curr.glitch_en; }
    void set_glitch_en(bool en) { curr.glitch_en = en; }

    // Skew state getters (for monitoring)
    uint8_t get_skew_counter() const { return curr.skew_counter; }
    bool get_skew_pending() const { return curr.skew_pending; }
    bool get_skew_target() const { return curr.skew_target; }

    // Timing monitor getters/setters
    bool get_tmon_en() const { return curr.tmon_en; }
    void set_tmon_en(bool en) { curr.tmon_en = en; }

    bool get_tmon_trig() const { return curr.tmon_trig; }
    void set_tmon_trig(bool trig) { curr.tmon_trig = trig; }

    uint16_t get_tmon_ctr() const { return curr.tmon_ctr; }
    void set_tmon_ctr(uint16_t ctr) { curr.tmon_ctr = ctr; }

    // Configuration getters
    MODE get_mode() const { return curr.mode; }
    uint32_t get_n() const { return curr.n; }
    uint8_t get_pckt_len() const { return curr.pckt_len; }
    uint32_t get_counter() const { return curr.counter; }

    // Internal state getters (for debugging)
    uint8_t get_osr() const { return curr.osr; }
    uint8_t get_bit_cnt() const { return curr.bit_cnt; }
    bool get_pin_oe() const { return curr.pin_oe; }
    bool get_pin_in_sync2() const { return curr.pin_in_sync2; }
    bool get_pin_out_actual() const { return curr.pin_out_actual; }

    void comb() {
        next = curr;

        fifo_push_pending = false;
        fifo_pop_pending = false;
        fifo_push_data = 0;

        // 2-stage synchronization of external pin_in
        next.pin_in_sync1 = pin_in;
        next.pin_in_sync2 = curr.pin_in_sync1;
        const bool pin_in_sync = curr.pin_in_sync2;

        next.pin_in_prev = pin_in_sync;
        next.flag_level = pin_in_sync;
        next.flag_negedge = (!pin_in_sync && curr.pin_in_prev);
        next.flag_posedge = (pin_in_sync && !curr.pin_in_prev);

        // Timing monitor
        if (curr.tmon_trig && curr.tmon_ctr < 0xFFFF) {
            next.tmon_ctr = curr.tmon_ctr + 1;
        }
        if ((!pin_in_sync && curr.pin_in_prev) || (pin_in_sync && !curr.pin_in_prev)) {
            next.tmon_trig = !next.tmon_trig;
        }

        // Counter decrement and flag_ctr_zero evaluation
        const bool terminal = (curr.counter == 0);
        next.counter = terminal ? curr.n : (curr.counter - 1u);
        next.flag_ctr_zero = (next.counter == 0);

        switch (curr.mode) {
            case MODE::RELEASE:
                next.pin_oe = false;
                break;

            case MODE::WAIT:
                break;

            case MODE::SET0:
                next.pin_oe = true;
                next.pin_out = false;
                break;

            case MODE::SET1:
                next.pin_oe = true;
                next.pin_out = true;
                break;

            case MODE::SHIFT_IN:
                next.pin_oe = false;
                if (terminal) {
                    const uint8_t osr_u = static_cast<uint8_t>((curr.osr << 1) | (pin_in_sync ? 1u : 0u));
                    const uint8_t bit_cnt_u = curr.bit_cnt + 1;

                    if (bit_cnt_u >= curr.pckt_len) {
                        // Word complete (1..8 bits). Upper bits are zero-padded.
                        if (fifo.size() < FIFO_SIZE) {
                            fifo_push_pending = true;
                            fifo_push_data = osr_u;
                        }
                        next.osr = 0;
                        next.bit_cnt = 0;
                    } else {
                        next.osr = osr_u;
                        next.bit_cnt = bit_cnt_u;
                    }
                }
                break;

            case MODE::SHIFT_OUT:
                next.pin_oe = true;
                if (terminal) {
                    if (curr.bit_cnt == 0) {
                        if (!fifo.empty()) {
                            const uint8_t raw = fifo.front(); // Peek FIFO
                            fifo_pop_pending = true;

                            // Drive MSB of the pckt_len field (bit position pckt_len - 1)
                            const uint8_t msb_bit = curr.pckt_len - 1;
                            next.pin_out = ((raw >> msb_bit) & 1u) != 0;
                            next.osr = raw;
                            next.bit_cnt = msb_bit;
                        } else {
                            next.pin_out = false;
                        }
                    } else {
                        // Drive next payload bit
                        const uint8_t next_bit = curr.bit_cnt - 1;
                        next.pin_out = ((curr.osr >> next_bit) & 1u) != 0;
                        next.bit_cnt = next_bit;
                    }
                }
                break;

            case MODE::CLK_GEN:
                next.pin_oe = true;
                if (terminal) {
                    next.pin_out = !curr.pin_out;
                }
                break;

            case MODE::BAUD_GEN:
                next.pin_oe = true;
                next.pin_out = terminal;
                break;
        }

        // Fault Injection Engine
        if (curr.fault_en) {
            next.pin_out_actual = next.pin_out; // FSM clean output
            bool faulted_out = next.pin_out_actual;

            // 1. Skew Delay Generator
            if (curr.skew_en && curr.skew_cnt > 0) {
                if (!curr.skew_pending) {
                    if (next.pin_out_actual != curr.pin_out) {
                        // Edge detected: hold old value and begin delay
                        next.skew_pending = true;
                        next.skew_target = next.pin_out_actual;
                        next.skew_counter = curr.skew_cnt;
                        faulted_out = curr.pin_out;
                    }
                } else {
                    if (curr.skew_counter > 1) {
                        next.skew_counter = curr.skew_counter - 1;
                        faulted_out = curr.pin_out; // Continue holding old value
                    } else {
                        next.skew_counter = 0;
                        next.skew_pending = false;
                        faulted_out = curr.skew_target; // Release new edge
                    }
                }
            } else {
                next.skew_pending = false;
                next.skew_counter = 0;
            }

            // 2. Continuous Bit Flip
            if (curr.bit_flip) {
                faulted_out = !faulted_out;
            }

            // 3. 1-Cycle Terminal Glitch Pulse
            if (curr.glitch_en && terminal) {
                faulted_out = !faulted_out;
            }

            next.pin_out = faulted_out;
        }
    }

    void seq() {
        if (fifo_pop_pending && !fifo.empty()) {
            fifo.pop();
        }
        if (fifo_push_pending && fifo.size() < FIFO_SIZE) {
            fifo.push(fifo_push_data);
        }

        curr = next;

        fifo_push_pending = false;
        fifo_pop_pending = false;
    }

    void posedge() {
        seq();
        comb();
    }
};

// Timing monitor currently a stub
