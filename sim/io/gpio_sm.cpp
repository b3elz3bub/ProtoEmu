#include <cstdint>
#include <queue>

static constexpr size_t FIFO_SIZE = 32;
// Counter is assumed to be 32-bit here

enum class MODE {
    RELEASE,    // lets external slave drive the pin
    WAIT,       // waits for edge/level specified and sets flag
    SET0,       // sets the pin to LOW
    SET1,       // sets the pin to HIGH
    SHIFT_IN,   // shifts data in to FIFO from pin for N specified cycles
    SHIFT_OUT,  // shifts data out of FIFO onto pin for N specified cycles
    CLK_GEN,    // sets pin to CLK mode of f/N where N is specified
    BAUD_GEN    // sets pin to produce a tick every N/f interval
};

class GPIO_SM {
private:
    // State
    MODE mode;
    uint32_t n;
    bool flag_posedge;
    bool flag_negedge;
    bool flag_level;
    // Standard blocks
    uint32_t counter;
    std::queue<bool> fifo;
    // Outputs
    bool pin_out;
    bool pin_oe;
    // Input from physical/external pin
    bool pin_in;

    //NEXT STATE variables
    uint32_t counter_n;

public:
    GPIO_SM()
        : mode(MODE::RELEASE),
          n(0),
          flag_posedge(false),
          flag_negedge(false),
          flag_level(false),
          counter(0)
    {}

    // IO Ops
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
    bool get_flag_posedge(){
        return flag_posedge;
    }
    bool get_flag_negedge(){
        return flag_negedge;
    }
    bool get_flag_level(){
        return flag_level;
    }
    // Counter is completely private to GPIO_SM
    void set_mode(MODE mode){
        this->mode = mode;
    }
    void set_n(uint32_t n){
        this->n = n;
    }
    // Pin control is private to GPIO_SM

    //Internal operations
    void comb(){
        switch(mode){
            case MODE::RELEASE:
               pin_oe = 0;
               break;
            case MODE::WAIT:
                break;
            case MODE::SET0:
                pin_oe=1;
                pin_out=0;
                break;
            case MODE::SET1:
                pin_oe=1;
                pin_out=1;
                break;
            case MODE::SHIFT_IN:
                pin_oe=0;
                if(counter==0) counter_n=n;
                else counter_n=counter-1;
                break; //sequential only
            case MODE::SHIFT_OUT:
                if(counter==0) counter_n=n;
                else counter_n=counter-1;
                pin_oe=1;
                break;
            case MODE::CLK_GEN:
                pin_oe=1;
                if(counter==0) counter_n=n;
                else counter_n=counter-1;
                break;
            case MODE::BAUD_GEN:
                pin_oe=1;
                if(counter==0) counter_n=n;
                else counter_n=counter-1;
                break;
        }
    }
    void seq(){
        switch(mode){
            case MODE::RELEASE:
               pin_oe = 0;
               break;
            case MODE::WAIT:
                break;
            case MODE::SET0:
                pin_oe=1;
                pin_out=0;
                break;
            case MODE::SET1:
                pin_oe=1;
                pin_out=1;
                break;
            case MODE::SHIFT_IN:
                counter = counter_n;
                break; //sequential only
            case MODE::SHIFT_OUT:
                pin_oe=1;
                counter = counter_n;
                break;
            case MODE::CLK_GEN:
                pin_out = ~pin_out;
                counter = counter_n;
                break;
            case MODE::BAUD_GEN:
                pin_out=1;
                pin_out=0;
                counter = counter_n;
                break;
        }
    }
};
