#ifndef PPI_H
#define PPI_H

#include <cstdint>
#include <functional>

class PPI {
public:
    static PPI& getInstance();
    void reset();
    
    uint8_t read(uint8_t port);
    void write(uint8_t port, uint8_t value);

    // Callbacks for Port A (Primary Slot Select)
    void setSlotSelectCallback(std::function<void(uint8_t)> callback) { slot_select_callback = callback; }
    
    // Callbacks for Port C (Keyboard row, etc.)
    void setKeyboardRowSelectCallback(std::function<void(uint8_t)> callback) { row_select_callback = callback; }

private:
    PPI();
    uint8_t portA; // Primary Slot Select (Output)
    uint8_t portB; // Keyboard Read (Input)
    uint8_t portC; // Row Select, Cassette, Clicker (Mixed)
    uint8_t control;

    std::function<void(uint8_t)> slot_select_callback;
    std::function<void(uint8_t)> row_select_callback;
};

#endif
