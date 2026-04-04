#include "ppi.h"
#include "keyboard/keyboard.h"
#include <iostream>
#include <iomanip>

PPI& PPI::getInstance() {
    static PPI instance;
    return instance;
}

PPI::PPI() : portA(0), portB(0xFF), portC(0), control(0) {
    reset();
}

void PPI::reset() {
    portA = 0xF0; // Primary slot: Page 0=0, Page 1=0, Page 2=3, Page 3=3 (RAM in pages 2 and 3)
    portB = 0xFF; // No keys pressed
    portC = 0x00;
    // Notify memory about initial slot mapping
    if (slot_select_callback) {
        std::cout << "PPI::reset: Calling slot_select_callback with 0x" << std::hex << (int)portA << std::dec << std::endl;
        slot_select_callback(portA);
    }
}

uint8_t PPI::read(uint8_t port) {
    switch (port & 0x03) {
        case 0: return portA;
        case 1: {
            // Port B is Keyboard data (Input)
            // It should return the matrix state for the row selected in Port C
            uint8_t row = portC & 0x0F;
            return Keyboard::getInstance().readRow(row);
        }
        case 2: return portC;
        case 3: return control;
    }
    return 0xFF;
}

void PPI::write(uint8_t port, uint8_t value) {
    static int ppi_write_count = 0;
    if (ppi_write_count < 10) {
        std::cout << "PPI write: port=0x" << std::hex << (int)(port & 0x03) 
                  << " value=0x" << std::setw(2) << std::setfill('0') << (int)value 
                  << std::dec << std::endl;
        ppi_write_count++;
    }
    switch (port & 0x03) {
        case 0: // Port A: Primary Slot Select (AAAA BBBB CCCC DDDD)
            portA = value;
            if (slot_select_callback) {
                std::cout << "PPI: Calling slot_select_callback with value=0x" 
                          << std::hex << std::setw(2) << std::setfill('0') << (int)value 
                          << std::dec << std::endl;
                slot_select_callback(value);
            }
            break;
        case 1: // Port B is usually input on MSX
            portB = value;
            break;
        case 2: // Port C: Row Select (bits 0-3)
            portC = value;
            if (row_select_callback) row_select_callback(value & 0x0F);
            break;
        case 3: // Control register
            control = value;
            // Handle bit set/reset if bit 7 is 0
            if (!(value & 0x80)) {
                uint8_t bit = (value >> 1) & 0x07;
                if (value & 0x01) portC |= (1 << bit);
                else portC &= ~(1 << bit);
            }
            break;
    }
}
