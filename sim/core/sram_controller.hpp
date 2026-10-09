#pragma once

#include <stdint.h>
#include "sram.hpp"

// CPU requests use word addresses.
struct MemoryRequest {
    bool valid = false;
    uint16_t addr = 0;
    uint16_t wdata = 0;
    bool is_write = false;
    uint8_t wmask = 0b11;
};

// valid means a response is available this cycle.
struct MemoryResponse {
    uint16_t rdata = 0;
    bool valid = false;
};

// Single-port arbiter: data requests have priority over instruction fetch.
class SramController {
public:
    static constexpr uint16_t ADDR_MASK = OpenRAM_512x16::ADDR_MASK;

    bool fetch_ready = false;
    bool data_ready = false;
    bool stall_fetch = false;

    // Drive SRAM pins and determine which request is accepted.
    void comb(
        const MemoryRequest& fetch_req,
        const MemoryRequest& data_req,
        SramPins& ram_pins
    ) {
        ram_pins.csb = true;
        ram_pins.web = true;
        ram_pins.wmask = 0b11;
        ram_pins.addr = 0;
        ram_pins.din = 0;
        // Preserve ram_pins.dout.

        fetch_ready = false;
        data_ready = false;
        stall_fetch = false;

        selected_owner_ = Owner::NONE;
        selected_write_ = false;

        if (data_req.valid) {
            // Avoid two data responses colliding in one cycle.
            if (pending_read_owner_ == Owner::DATA) {
                stall_fetch = fetch_req.valid;
                return;
            }

            selected_owner_ = Owner::DATA;
            selected_write_ = data_req.is_write;
            data_ready = true;

            ram_pins.csb = false;
            ram_pins.web = !data_req.is_write;
            ram_pins.wmask =
                static_cast<uint8_t>(data_req.wmask & 0b11u);
            ram_pins.addr =
                static_cast<uint16_t>(data_req.addr & ADDR_MASK);
            ram_pins.din = data_req.wdata;

            if (fetch_req.valid) stall_fetch = true;
        } else if (fetch_req.valid) {
            selected_owner_ = Owner::FETCH;
            fetch_ready = true;

            ram_pins.csb = false;
            ram_pins.web = true;
            ram_pins.wmask = 0b11;
            ram_pins.addr =
                static_cast<uint16_t>(fetch_req.addr & ADDR_MASK);
        }
    }

    // Call before sram.step(true), so the previous read data is still present.
    void seq(
        bool rising_edge,
        const SramPins& ram_pins,
        MemoryResponse& fetch_resp,
        MemoryResponse& data_resp
    ) {
        if (!rising_edge) return;

        fetch_resp.valid = false;
        data_resp.valid = false;

        // Complete the previous cycle's read.
        if (pending_read_owner_ == Owner::FETCH) {
            fetch_resp.rdata = ram_pins.dout;
            fetch_resp.valid = true;
        } else if (pending_read_owner_ == Owner::DATA) {
            data_resp.rdata = ram_pins.dout;
            data_resp.valid = true;
        }

        // Track the current accepted request.
        switch (selected_owner_) {
            case Owner::DATA:
                if (selected_write_) {
                    data_resp.rdata = 0;
                    data_resp.valid = true;
                    pending_read_owner_ = Owner::NONE;
                } else {
                    pending_read_owner_ = Owner::DATA;
                }
                break;

            case Owner::FETCH:
                pending_read_owner_ = Owner::FETCH;
                break;

            case Owner::NONE:
            default:
                pending_read_owner_ = Owner::NONE;
                break;
        }
    }

private:
    enum class Owner : uint8_t {
        NONE,
        FETCH,
        DATA
    };

    Owner pending_read_owner_ = Owner::NONE;
    Owner selected_owner_ = Owner::NONE;
    bool selected_write_ = false;
};
