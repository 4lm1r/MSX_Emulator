#ifndef OPCODES_HANDLER_H
#define OPCODES_HANDLER_H

#include <cstdint>
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

    void handleDataTransfer(uint8_t opcode);
    void handleArithmetic(uint8_t opcode);
    void handleLogic(uint8_t opcode);
    void handleControlFlow(uint8_t opcode);
    void handleIO(uint8_t opcode);
    void handleBitManip(uint8_t opcode);
    void handleStack(uint8_t opcode);
    void handleIXInstructions(uint8_t opcode);
    void handleIYInstructions(uint8_t opcode);

    uint8_t readMemory(uint16_t addr);
    void writeMemory(uint16_t addr, uint8_t value);
    uint8_t readIO(uint8_t port);
    void writeIO(uint8_t port, uint8_t value);
    int handleBitInstructions(uint8_t opcode);
};

#endif
