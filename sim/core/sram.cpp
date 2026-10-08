#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <sstream>

class OpenRAM_2KB {
public:
    static constexpr size_t WORDS = 1024; // 1024 x 16-bit = 2KB
    static constexpr uint16_t ADDR_MASK = WORDS - 1;

    // Interface Pins
    struct Pins {
        bool     clk   = false;
        bool     csb   = true;   // Active-LOW Chip Select
        bool     web   = true;   // Active-LOW Write Enable
        uint8_t  wmask = 0b11;   // 2-bit byte mask [upper_byte, lower_byte]
        uint16_t addr  = 0;      // 10-bit address (0..1023)
        uint16_t din   = 0;      // Write data
        uint16_t dout  = 0;      // Read data output
    } io;

private:
    std::vector<uint16_t> mem;
    uint16_t dout_reg = 0x0000;  // Synchronous output latch

public:
    OpenRAM_2KB() : mem(WORDS, 0x0000) {}

    // Model edge-triggered evaluation on rising edge of clock
    void step(bool rising_edge) {
        if (!rising_edge) return;

        // OpenRAM is disabled if csb is HIGH (1)
        if (io.csb) {
            return;
        }

        uint16_t valid_addr = io.addr & ADDR_MASK;

        if (!io.web) {
            // --- WRITE OPERATION (web == 0) ---
            uint16_t current_val = mem[valid_addr];
            uint16_t write_val   = current_val;

            if (io.wmask & 0b01) { // Write lower byte [7:0]
                write_val = (write_val & 0xFF00) | (io.din & 0x00FF);
            }
            if (io.wmask & 0b10) { // Write upper byte [15:8]
                write_val = (write_val & 0x00FF) | (io.din & 0xFF00);
            }

            mem[valid_addr] = write_val;

            // OpenRAM write behavior: dout holds previous data or undefined
            // We maintain dout_reg state
        }
        else {
            // --- READ OPERATION (web == 1) ---
            // Read data sampled on rising edge, registered to output
            dout_reg = mem[valid_addr];
        }

        // Drive output bus
        io.dout = dout_reg;
    }

    // Helper: Direct backdoor access for simulator inspection
    uint16_t direct_read(uint16_t addr) const {
        return mem[addr & ADDR_MASK];
    }

    void direct_write(uint16_t addr, uint16_t data) {
        mem[addr & ADDR_MASK] = data;
    }

    // Load assembled Verilog/Hex firmware into SRAM
    bool load_hex(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line;
        uint16_t addr = 0;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#' || line[0] == '/') continue;
            std::stringstream ss(line);
            uint32_t val;
            if (ss >> std::hex >> val) {
                if (addr < WORDS) {
                    mem[addr++] = static_cast<uint16_t>(val);
                }
            }
        }
        return true;
    }
};
