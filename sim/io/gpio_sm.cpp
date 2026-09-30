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
    bool flag_edge;
    bool flag_level;
    // Standard blocks
    uint32_t counter;
    std::queue<bool> fifo;
    // Outputs
    bool pin_out;
    // Input from physical/external pin
    bool pin_in;

public:
    GPIO_SM()
        : mode(MODE::RELEASE),
          n(0),
          flag_edge(false),
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
    bool get_flag_edge(){
        return flag_edge;
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
};
