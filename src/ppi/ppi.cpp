#include "ppi.h"
#include "keyboard/keyboard.h"
#include "memory/memory.h"
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
    portA = 0xF0; // Primary slot: Page 0=0, Page 1=0, Page 2=3, Page 3=3
    portB = 0xFF; // Keyboard data
    portC = 0x00;
    // Notify memory about initial slot mapping
    Memory::getInstance().mapPrimarySlots(portA);
}

uint8_t PPI::read(uint8_t port) {
    switch (port & 0x03) {
        case 0: return portA;
        case 1: {
            uint8_t row = portC & 0x0F;
            return Keyboard::getInstance().readRow(row);
        }
        case 2: return portC;
        case 3: return control;
    }
    return 0xFF;
}

void PPI::write(uint8_t port, uint8_t value) {
    switch (port & 0x03) {
        case 0:
            portA = value;
            Memory::getInstance().mapPrimarySlots(value);
            break;
        case 1:
            portB = value;
            break;
        case 2:
            portC = value;
            break;
        case 3:
            control = value;
            if (!(value & 0x80)) {
                uint8_t bit = (value >> 1) & 0x07;
                if (value & 0x01) portC |= (1 << bit);
                else portC &= ~(1 << bit);
            }
            break;
    }
}
