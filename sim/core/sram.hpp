#pragma once
#include <cstdint>
#include <array>

struct MemoryRequest {
    bool     valid    = false;
    uint16_t addr     = 0;
    uint32_t wdata    = 0;
    bool     is_write = false;
    uint32_t wmask    = 0xFFFFFFFFu;  // per-bit mask (A_BM), full word by default
};

struct MemoryResponse {
    uint32_t rdata = 0;
    bool     ready = false;
};

class SRAM_512x32 {
public:
    static constexpr std::size_t   WORDS     = 512;
    static constexpr uint16_t ADDR_MASK = WORDS - 1;

    struct Pins {
        bool     A_MEN  = false;
        bool     A_WEN  = false;
        bool     A_REN  = false;
        uint16_t A_ADDR = 0;
        uint32_t A_DIN  = 0;
        uint32_t A_BM   = 0;
        uint32_t A_DOUT = 0;
    } io;

private:
    std::array<uint32_t, WORDS> mem{};

    // curr_read = the request sampled LAST edge; seq() produces A_DOUT
    // from it THIS edge -- this is what gives the correct one-cycle
    // registered latency. next_read = what comb() is staging THIS edge,
    // to be serviced on the edge after this one.
    struct ReadReq { bool pending = false; uint16_t addr = 0; };
    ReadReq curr_read{};
    ReadReq next_read{};

    bool     write_pending = false;
    uint16_t write_addr = 0;
    uint32_t write_data = 0;
    uint32_t write_mask = 0;

public:
    SRAM_512x32() = default;

    void comb() {
        write_pending = false;
        next_read = ReadReq{};

        if (io.A_MEN) {
            const uint16_t addr = io.A_ADDR & ADDR_MASK;
            if (io.A_WEN) {
                write_pending = true;
                write_addr = addr; write_data = io.A_DIN; write_mask = io.A_BM;
            }
            if (io.A_REN) {
                next_read.pending = true;
                next_read.addr = addr;
            }
        }
    }

    void seq() {
        if (write_pending) {
            uint32_t cur = mem[write_addr];
            mem[write_addr] = (cur & ~write_mask) | (write_data & write_mask);
        }
        if (curr_read.pending) {
            // Write-through falls out naturally here: if A_WEN and A_REN
            // were both set on the SAME edge as this read's request, the
            // write above committed to mem[] one full edge before this
            // deferred read actually fires, so it already sees the new
            // data. Matches Ken's page ("both high is a write-through"),
            // but confirm against the macro's own .txt datasheet for the
            // exact guarantee, not just this derivation.
            io.A_DOUT = mem[curr_read.addr];
        }
        curr_read = next_read;
        write_pending = false;
    }

    void posedge() { comb(); seq(); }

    uint32_t direct_read(uint16_t addr) const { return mem[addr & ADDR_MASK]; }
    void direct_write(uint16_t addr, uint32_t data) { mem[addr & ADDR_MASK] = data; }
};
