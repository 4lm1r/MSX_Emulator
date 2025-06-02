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
    void InitializeOpcodeMap();
    int getCycles() const; // Added to retrieve cycle count

private:
    Z80A& cpu;
    int cycles; // Added member variable to track instruction cycles
    
    using OpcodeFunction = void (OpcodesHandler::*)(uint8_t);
    std::unordered_map<uint8_t, OpcodeFunction> dataTransfer;
    std::unordered_map<uint8_t, OpcodeFunction> handleIY;
    std::unordered_map<uint8_t, OpcodeFunction> handleIX;
    
    void IY_OXE5(uint8_t);
    void IY_OX77(uint8_t);
    void IY_OX7E(uint8_t);
    void IX_OX77(uint8_t);
    void IX_OX74(uint8_t);
    void IX_OX75(uint8_t);
    void IX_OX7E(uint8_t);
    void IX_OXE5(uint8_t);
    void DT_OX3E(uint8_t);
    void DT_OXO6(uint8_t);
    void DT_OXOE(uint8_t);
    void DT_OXO1(uint8_t);
    void DT_OX21(uint8_t);
    void DT_OX77(uint8_t);
    void DT_OX7E(uint8_t);
    void DT_OXOA(uint8_t);
    void DT_OXO2(uint8_t);
    void DT_OX47(uint8_t);
    void DT_OX4D(uint8_t);
    void DT_OX61(uint8_t);
    void DT_OX3A(uint8_t);
    void DT_OX56(uint8_t);
    void DT_OX4F(uint8_t);
    void DT_OX36(uint8_t);
    void DT_OX2E(uint8_t);
    void DT_OX32(uint8_t);
    void DT_OX7D(uint8_t);
    void DT_OX78(uint8_t);
    void DT_OX67(uint8_t);
    void DT_OX6F(uint8_t);

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
