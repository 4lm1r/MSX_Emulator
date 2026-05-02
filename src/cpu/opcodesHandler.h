#ifndef OPCODES_HANDLER_H
#define OPCODES_HANDLER_H

#include <cstdint>
#include <unordered_map>
#include "debug.h"

class Z80A;

class OpcodesHandler {
public:
    OpcodesHandler(Z80A& cpu);
    void executeOpcode(uint8_t opcode);
    int getCycles() const; // Added to retrieve cycle count

private:
    Z80A& cpu;
    int cycles; // Added member variable to track instruction cycles
                
    // Helper to update Sign, Zero, and PArity flags based on a result
    void updateSZP(uint8_t result);

    //Helper for 8-bit Addition flags (sets S, Z, H, V, N, C)
    void updateFlagsAdd(uint8_t a, uint8_t b, uint16_t result);

    //Helper for 8-bit Subtraction flags (sets S, Z, H, V, N, C)
    void updateFlagsSub(uint8_t a, uint8_t b, uint16_t result);

    void updateFlagsAdc(uint8_t a, uint8_t b, uint16_t result);
    void updateFlagsSbc(uint8_t a, uint8_t b, uint16_t result);

    // Helper to read 16-bit immediate values from memory (useful for many opcpdes)
    uint16_t read16(uint16_t addr);

    int handleCB(uint8_t opcode);
    int handleED(uint8_t opcode);
    int handleIX(uint8_t opcode);
    int handleIY(uint8_t opcode);
    int handleDDCB(uint8_t opcode, int8_t d);
    int handleFDCB(uint8_t opcode, int8_t d);

    uint8_t readMemory(uint16_t addr);
    void writeMemory(uint16_t addr, uint8_t value);
    uint8_t readIO(uint8_t port);
    void writeIO(uint8_t port, uint8_t value);
};

#endif
