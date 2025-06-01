#include "opcodesHandler.h"
#include "cpu/z80a.h"
#include <iostream>

OpcodesHandler::OpcodesHandler(Z80A& cpu) : cpu(cpu) {}

void OpcodesHandler::executeOpcode(uint8_t opcode) {
    std::cout << "Executing opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
    switch ((opcode & 0xC0) >> 6) { // Basic grouping by high 2 bits
        case 0x00: handleDataTransfer(opcode); break;
        case 0x01: handleArithmetic(opcode); break;
        case 0x02: handleLogic(opcode); break;
        case 0x03: handleControlFlow(opcode); break;
        default: handleBitManip(opcode); break; // Simplified for now
    }
}

void OpcodesHandler::handleDataTransfer(uint8_t opcode) {
    switch (opcode) {
        case 0x3E: { // LD A, n
            uint8_t value = readMemory(cpu.PC + 1);
            cpu.A = value;
            cpu.PC += 2;
            std::cout << "LD A, 0x" << std::hex << (int)value << std::dec << std::endl;
            break;
        }
        // Add more (e.g., LD B, n, LD HL, nn)
        default:
            std::cout << "Unhandled data transfer opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
    }
}

void OpcodesHandler::handleArithmetic(uint8_t opcode) {
    switch (opcode) {
        case 0x80: // ADD A, B
            cpu.A += cpu.B;
            cpu.F = (cpu.A == 0) ? 0x80 : 0; // Zero flag
            cpu.PC++;
            std::cout << "ADD A, B" << std::endl;
            break;
        // Add more (e.g., SUB, ADC)
        default:
            std::cout << "Unhandled arithmetic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
    }
}

void OpcodesHandler::handleLogic(uint8_t opcode) {
    switch (opcode) {
        case 0xA0: // AND B
            cpu.A &= cpu.B;
            cpu.F = (cpu.A == 0) ? 0x80 : 0; // Zero flag
            cpu.PC++;
            std::cout << "AND B" << std::endl;
            break;
        // Add more (e.g., OR, XOR)
        default:
            std::cout << "Unhandled logic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
    }
}

void OpcodesHandler::handleControlFlow(uint8_t opcode) {
    switch (opcode) {
        case 0xC3: { // JP nn
            uint16_t addr = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.PC = addr;
            std::cout << "JP 0x" << std::hex << addr << std::dec << std::endl;
            break;
        }
        // Add more (e.g., CALL, RET)
        default:
            std::cout << "Unhandled control flow opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
    }
}

void OpcodesHandler::handleBitManip(uint8_t opcode) {
    std::cout << "Unhandled bit manipulation opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
    // Add BIT, RES, SET later
}

uint8_t OpcodesHandler::readMemory(uint16_t addr) {
    return cpu.memoryReadCallback(addr);
}

void OpcodesHandler::writeMemory(uint16_t addr, uint8_t value) {
    cpu.memoryWriteCallback(addr, value);
}

uint8_t OpcodesHandler::readIO(uint8_t port) {
    return cpu.ioReadCallback(port);
}

void OpcodesHandler::writeIO(uint8_t port, uint8_t value) {
    cpu.ioWriteCallback(port, value);
}
