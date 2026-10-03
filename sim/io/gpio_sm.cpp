#include <cstdint>
#include <queue>

static constexpr size_t FIFO_SIZE = 32;
// Counter is assumed to be 32-bit here

enum class MODE {
    RELEASE,    // lets external slave drive the pin (OEN=0)
    WAIT,       // waits for edge/level specified and sets flag
    SET0,       // Drives the pin to LOW
    SET1,       // Drives the pin to HIGH
    SHIFT_IN,   // shifts data in to FIFO from pin every Nth cycle
    SHIFT_OUT,  // shifts data out of FIFO onto pin every Nth cycle
    CLK_GEN,    // sets pin to CLK mode of f/2N where N is specified
    BAUD_GEN    // sets pin to produce a tick every N/f interval
};

class GPIO_SM {
private:
    struct Registers{
        MODE mode = MODE::RELEASE;
        uint32_t n = 0;
        bool flag_posedge = false;
        bool flag_negedge = false;
        bool flag_level = false;
        // Standard blocks
        uint32_t counter = 0;
        // Outputs
        bool pin_out = false;
        bool pin_oe = false;
        // Input from physical/external pin
        bool pin_in_prev = 0;

        // Packet Sizes and Counters
        uint32_t osr = 0; // DATA Shifted into/out of fifo
        uint8_t bit_cnt = 0;
    };
    Registers curr;
    Registers next;
    std::queue<uint32_t> fifo;

public:
    bool pin_in = 0;
    GPIO_SM() = default;

    void config(MODE mode, uint32_t n){
        next.mode = mode;
        next.n = n;
        next.counter = n;
    }

    // GET,SET FIFO methods assume 32 bit FIFO
    uint32_t get_fifo(){
        uint32_t val = 0;
            for (int i = 0; i < FIFO_SIZE && !fifo.empty(); i++){
                val = (val << 1) | fifo.front();
                fifo.pop();
            }
            return val;
    }
    void set_fifo(uint32_t val){
        for(int i=31;i>=0;i--){
            fifo.push((val>>i)&1);
        }
    }
    // Getters
    bool get_flag_posedge(){return curr.flag_posedge;}
    bool get_flag_negedge(){return curr.flag_negedge;}
    bool get_flag_level(){return curr.flag_level;} // Latched value
    bool get_pin_out(){return curr.pin_out;} // Asynchrnous value

    // Counter is completely private to GPIO_SM
    // Pin control is private to GPIO_SM

    //Internal operations
    void comb(){
        next = curr; // defaults
        switch(curr.mode){
            case MODE::RELEASE:
               next.pin_oe = 0;
               break;
            case MODE::WAIT:
                break;
            case MODE::SET0:
                next.pin_oe = 1;
                next.pin_out = 0;
                break;
            case MODE::SET1:
                next.pin_oe = 1;
                next.pin_out = 1;
                break;
            case MODE::SHIFT_IN:
                next.pin_oe = 0;
                if(curr.counter == 0){
                    next.counter = curr.n;
                    next.osr = pin_in;
                    next.osr = (curr.osr << 1) | (pin_in ? 1 : 0);
                    next.bit_cnt = curr.bit_cnt + 1;
                    if(next.bit_cnt == 32){
                        if(fifo.size() < FIFO_SIZE) fifo.push(next.osr);
                        next.bit_cnt=0;
                    }
                }
                else next.counter = curr.counter - 1;
                break;
            case MODE::SHIFT_OUT:
                next.pin_oe = 1;
                if(curr.counter == 0){
                    next.counter = curr.n;
                    if(curr.bit_cnt==0 && !fifo.empty()){
                        next.osr = fifo.front();
                        fifo.pop();
                        next.bit_cnt = 32;
                    }
                    if(curr.bit_cnt > 0){
                        next.pin_out = curr.osr >> 31 & 1;
                        next.osr = curr.osr << 1;
                        next.bit_cnt = curr.bit_cnt - 1;
                    }
                }
                break;
            case MODE::CLK_GEN:
                next.pin_oe = 1;
                if(curr.counter==0){
                    next.pin_out = ~curr.pin_out;
                    next.counter = curr.n;
                }
                else next.counter = curr.counter - 1;
                break;
            case MODE::BAUD_GEN:
                next.pin_oe = 1;
                if(curr.counter == 0){
                    next.pin_out = 1;
                    next.counter = curr.n;
                }
                else{
                    next.pin_out = 0;
                    next.counter = curr.counter - 1;
                }
                break;
        }
    }
    void seq(){
        next.pin_in_prev = pin_in;
        curr = next;

        // Flag updates
        next.flag_level = pin_in;
        next.flag_negedge = (!pin_in && curr.pin_in_prev);
        next.flag_posedge = (pin_in && !curr.pin_in_prev);
    }
};
