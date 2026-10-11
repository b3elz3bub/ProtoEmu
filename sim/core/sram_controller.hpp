#pragma once
#include "sram.hpp"

class SramController {
private:
    enum class LastOwner { NONE, FETCH, DATA };
    LastOwner pending_owner = LastOwner::NONE;  // whose request resolves THIS cycle

public:
    bool stall_fetch = false;

    void comb(const MemoryRequest& fetch_req,
              const MemoryRequest& data_req,
              SRAM_512x32::Pins& ram_pins)
    {
        ram_pins.A_MEN = false; ram_pins.A_WEN = false; ram_pins.A_REN = false;
        ram_pins.A_ADDR = 0; ram_pins.A_DIN = 0; ram_pins.A_BM = 0;
        stall_fetch = false;

        if (data_req.valid) {
            ram_pins.A_MEN = true;
            ram_pins.A_WEN = data_req.is_write;
            ram_pins.A_REN = !data_req.is_write;
            ram_pins.A_ADDR = data_req.addr & SRAM_512x32::ADDR_MASK;
            ram_pins.A_DIN  = data_req.wdata;
            ram_pins.A_BM   = data_req.wmask;
            if (fetch_req.valid) stall_fetch = true;
        } else if (fetch_req.valid) {
            ram_pins.A_MEN = true;
            ram_pins.A_REN = true;
            ram_pins.A_ADDR = fetch_req.addr & SRAM_512x32::ADDR_MASK;
        }
    }

    void seq(const MemoryRequest& fetch_req,
             const MemoryRequest& data_req,
             const SRAM_512x32::Pins& ram_pins,
             MemoryResponse& fetch_resp,
             MemoryResponse& data_resp)
    {
        fetch_resp.ready = false;
        data_resp.ready  = false;

        if (pending_owner == LastOwner::DATA) {
            data_resp.rdata = ram_pins.A_DOUT; data_resp.ready = true;
        } else if (pending_owner == LastOwner::FETCH) {
            fetch_resp.rdata = ram_pins.A_DOUT; fetch_resp.ready = true;
        }

        if (data_req.valid && !data_req.is_write)      pending_owner = LastOwner::DATA;
        else if (!data_req.valid && fetch_req.valid)   pending_owner = LastOwner::FETCH;
        else                                             pending_owner = LastOwner::NONE;
    }

    void posedge(SRAM_512x32& ram,
                 const MemoryRequest& fetch_req, const MemoryRequest& data_req,
                 MemoryResponse& fetch_resp, MemoryResponse& data_resp)
    {
        comb(fetch_req, data_req, ram.io);
        ram.posedge();
        seq(fetch_req, data_req, ram.io, fetch_resp, data_resp);
    }
};
