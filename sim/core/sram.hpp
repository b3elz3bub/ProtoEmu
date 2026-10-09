#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Abstract simulator-side SRAM pins.
// These are NOT the literal IHP macro pins.
// The hardware wrapper must translate csb/web/byte wmask into the
// selected macro's MEN/REN/WEN/bit-mask signals.
struct SramPins {
    bool csb = true;            // Active-low chip select; true means disabled.
    bool web = true;            // Active-low write enable; false means write.
    std::uint8_t wmask = 0b11;  // Bit 0 enables low byte; bit 1 enables high byte.
    std::uint16_t addr = 0;     // Word address, not byte address.
    std::uint16_t din = 0;
    std::uint16_t dout = 0;
};

// Behavioral model for a 512 x 16-bit, single-port SRAM (1 KiB total).
// This class models behavior; it does not model analog timing, setup/hold
// violations, BIST, or power-up contents.
class OpenRAM_512x16 {
public:
    static constexpr std::size_t WORDS = 512;
    static constexpr std::uint16_t ADDR_MASK =
        static_cast<std::uint16_t>(WORDS - 1);

    SramPins io{};

    OpenRAM_512x16() : mem_(WORDS, 0) {}

    // Call once for each simulated rising edge, after the controller has
    // consumed the previous dout value. Disabled/write cycles hold dout.
    void step(bool rising_edge) {
        if (!rising_edge) {
            return;
        }

        if (!io.csb) {
            const std::size_t address =
                static_cast<std::size_t>(io.addr & ADDR_MASK);

            if (!io.web) {
                // Write selected byte lanes.
                // A set mask bit enables that lane.
                std::uint16_t value = mem_[address];

                if ((io.wmask & 0b01u) != 0u) {
                    value = static_cast<std::uint16_t>(
                        (value & 0xFF00u) | (io.din & 0x00FFu)
                    );
                }

                if ((io.wmask & 0b10u) != 0u) {
                    value = static_cast<std::uint16_t>(
                        (value & 0x00FFu) | (io.din & 0xFF00u)
                    );
                }

                mem_[address] = value;

                // Synchronous SRAM output holds its previous value on writes.
            } else {
                // Registered read: the selected word appears after this edge.
                dout_reg_ = mem_[address];
            }
        }

        io.dout = dout_reg_;
    }

    // Testbench-only backdoor access.
    // Do not model these as hardware ports.
    std::uint16_t direct_read(std::uint16_t word_addr) const {
        return mem_[
            static_cast<std::size_t>(word_addr & ADDR_MASK)
        ];
    }

    void direct_write(std::uint16_t word_addr, std::uint16_t data) {
        mem_[
            static_cast<std::size_t>(word_addr & ADDR_MASK)
        ] = data;
    }

    // Load one 16-bit hexadecimal word per line.
    // Blank lines and lines whose first non-space character is
    // '#', '/', or ';' are ignored.
    //
    // Returns false if the file cannot be opened, a word is invalid,
    // a word exceeds 16 bits, or the image exceeds SRAM capacity.
    bool load_hex(const std::string& filename) {
        std::ifstream file(filename);

        if (!file.is_open()) {
            return false;
        }

        std::string line;
        std::size_t address = 0;

        while (std::getline(file, line)) {
            const std::size_t first =
                line.find_first_not_of(" \t\r\n");

            if (first == std::string::npos) {
                continue;
            }

            const char marker = line[first];

            if (marker == '#' || marker == '/' || marker == ';') {
                continue;
            }

            std::stringstream stream(line.substr(first));
            std::uint32_t value = 0;

            if (!(stream >> std::hex >> value)) {
                return false;
            }

            if (value > 0xFFFFu || address >= WORDS) {
                return false;
            }

            mem_[address++] = static_cast<std::uint16_t>(value);
        }

        return !file.bad();
    }

private:
    std::vector<std::uint16_t> mem_;
    std::uint16_t dout_reg_ = 0;
};
