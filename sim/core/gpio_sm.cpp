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
    CLK_GEN,    // sets pin to CLK mode of f/2(N+1) where N is specified
    BAUD_GEN    // sets pin to produce a tick every (N+1)/f interval
};

class GPIO_SM {
private:
    struct Registers{
        MODE mode = MODE::RELEASE;
        uint32_t n = 0;
        bool flag_posedge = false;
        bool flag_negedge = false;
        bool flag_level = false;
        bool flag_ctr_zero = false;
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
        uint8_t pckt_len = 0; // Packet Length

        // Fault insertion
        bool fault_en = 0;
        bool skew_en = 0;
        uint8_t skew_cnt = 0;
        uint8_t skew_counter = 0;
        bool pin_out_saved = 0;
        bool bit_flip = 0;
        bool glitch_en = 0;
        bool pin_out_actual = 0;

        // Timing Monitor
        bool tmon_en = 0;
        bool tmon_trig = 0;
        uint16_t tmon_ctr = 0;

    };
    Registers curr;
    Registers next;
    std::queue<uint32_t> fifo;

public:
    bool pin_in = 0;
    GPIO_SM() = default;

    void config(MODE mode, uint32_t n, uint8_t pckt_len = 32){
        curr.mode = mode;
        curr.n = n;
        curr.pckt_len = (pckt_len > 32 || pckt_len == 0) ? 32 : pckt_len;
        curr.counter = n;
        curr.bit_cnt = 0;
        comb();
    }

    // GET,SET FIFO methods assume 32 bit FIFO
    uint32_t get_fifo(){
       if(fifo.empty()) return 0;
       else{
           uint32_t val = fifo.front();
           fifo.pop();
           return val;
       }
    }
    void set_fifo(uint32_t val){
        if(fifo.size()<FIFO_SIZE) fifo.push(val);
    }

    void set_pin_in(bool pin_in){
        this->pin_in = pin_in;
        comb();
    }
    // Getters
    bool get_flag_posedge(){return curr.flag_posedge;}
    bool get_flag_negedge(){return curr.flag_negedge;}
    bool get_flag_ctr_zero(){return curr.flag_ctr_zero;}
    bool get_flag_level(){return curr.flag_level;} // Latched value
    bool get_pin_out(){return curr.pin_out;} // Asynchrnous value

    // Counter is completely private to GPIO_SM
    // Pin control is private to GPIO_SM

    //Internal operations
    void comb(){
        next = curr; // defaults

        next.flag_level = pin_in;
        next.pin_in_prev = pin_in;
        next.flag_negedge = (!pin_in && curr.pin_in_prev);
        next.flag_posedge = (pin_in && !curr.pin_in_prev);

        if(curr.tmon_trig && curr.tmon_ctr < 0xFFFF) next.tmon_ctr = curr.tmon_ctr + 1;
        if(!pin_in && curr.pin_in_prev || pin_in && !curr.pin_in_prev) next.tmon_trig = !next.tmon_trig;

        next.counter = curr.counter - 1;
        if(curr.counter == 0) next.counter = curr.n;

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
                    uint32_t osr_u = (curr.osr << 1)|(pin_in? 1:0);
                    uint8_t bit_cnt_u = curr.bit_cnt + 1;
                    if(bit_cnt_u==curr.pckt_len){
                        if(fifo.size()<FIFO_SIZE){
                            fifo.push(osr_u);
                        }
                        next.bit_cnt = 0;
                        next.osr =0;
                    }
                    else{
                        next.osr = osr_u;
                        next.bit_cnt = bit_cnt_u;
                    }
                }
                break;
            case MODE::SHIFT_OUT:
                next.pin_oe = 1;
                if(curr.counter == 0){
                    uint32_t osr_u = curr.osr;
                    uint8_t bit_cnt_u = curr.bit_cnt;
                    if(curr.bit_cnt==0 && !fifo.empty()){
                        uint32_t raw = fifo.front();
                        fifo.pop();
                        uint8_t shftamt = 32-curr.pckt_len;
                        osr_u = raw<<shftamt;
                        bit_cnt_u = curr.pckt_len;
                        next.bit_cnt = bit_cnt_u;
                    }
                    else{
                        next.pin_out = (osr_u>>31) & 1;
                        next.osr = osr_u << 1;
                        next.bit_cnt = bit_cnt_u - 1;
                    }
                }
                next.pin_out = next.pin_out;
                break;
            case MODE::CLK_GEN:
                next.pin_oe = 1;
                if(curr.counter==0){
                    next.pin_out = !curr.pin_out;
                }
                break;
            case MODE::BAUD_GEN:
                next.pin_oe = 1;
                if(curr.counter == 0){
                    next.pin_out = 1;
                }
                else{
                    next.pin_out = 0;
                }
                break;
            }
        if(curr.fault_en){
            next.pin_out_actual = next.pin_out;
            if(curr.skew_en){
                if(curr.counter == 0){
                    next.pin_out_saved = next.pin_out;
                    next.pin_out = curr.pin_out;
                    next.skew_counter = curr.skew_cnt;
                }
                if(curr.skew_counter == 0) next.pin_out = next.pin_out_saved;
                next.skew_counter = curr.skew_counter - 1;
            }
            if(curr.bit_flip) next.pin_out = !next.pin_out;
            if(curr.counter == 0 && curr.glitch_en) next.pin_out = !next.pin_out;
            else if(curr.glitch_en) next.pin_out = next.pin_out_actual;
        }
        if(next.counter == 0) next.flag_ctr_zero = 1;
        else next.flag_ctr_zero = 0;
    }
    void seq(){
        curr = next;
    }
    void posedge(){
        seq();
        comb();
    }
};

// TO-DO:
// FIFO SHIFT IN and SHIFT OUT may be wrong
// check comb() after set_pin_in
// check if seq() after zero flag is correct
// Fifo Sizes need to be modified (8 bit word X depth)
// Verify State Description Counts
// Fix Skew and Glitch logic
