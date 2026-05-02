#include "z80a.h"
#include "opcodesHandler.h"
#include "debug.h"
#include <iomanip>
#include <iostream>

Z80A::Z80A() : A(0), B(0), C(0), D(0), E(0), H(0), L(0), F(0), PC(0), SP(0),
               A_(0), F_(0), B_(0), C_(0), D_(0), E_(0), H_(0), L_(0),
               IX(0), IY(0), I(0), R(0),
               halted(false), IFF1(false), IFF2(false),
               interruptPending(false), total_cycles(0),
               EI_pending(false)
{
    opcodeHandler = std::make_unique<OpcodesHandler>(*this);
}

Z80A::~Z80A() = default;

void Z80A::reset() {
    A = B = C = D = E = H = L = F = 0;
    A_ = F_ = B_ = C_ = D_ = E_ = H_ = L_ = 0;
    IX = IY = I = R = 0;
    PC = 0x0000;
    // SP is NOT initialized on real Z80 reset.
    halted = false;
    IFF1 = IFF2 = false;
    interruptPending = false;
    total_cycles = 0;
    EI_pending = false;
    //std::cout << "Z80A::reset: PC=0x" << std::hex << PC << std::dec << std::endl;
}

int Z80A::execute() {
    if (halted) {
        return 4;
    }

    bool trigger_ei = false;
    if (EI_pending) {
        trigger_ei = true;
        EI_pending = false;
    }
    
    uint16_t current_pc = PC;
    uint8_t opcode = memoryReadCallback(PC);
    debug_log << "PC: 0x" << std::hex << current_pc << " Op: 0x" << (int)opcode << std::dec << std::endl;
    PC++;

    opcodeHandler->executeOpcode(opcode);
    int cycles = opcodeHandler->getCycles();
    total_cycles += cycles;
    
    if (trigger_ei) {
        IFF1 = IFF2 = true;
    }
    
    return cycles;
}

void Z80A::setMemoryReadCallback(std::function<uint8_t(uint16_t)> callback) {
    memoryReadCallback = callback;
}

void Z80A::setMemoryWriteCallback(std::function<void(uint16_t, uint8_t)> callback) {
    memoryWriteCallback = callback;
}

void Z80A::setIOReadCallback(std::function<uint8_t(uint8_t)> callback) {
    ioReadCallback = callback;
}

void Z80A::setIOWriteCallback(std::function<void(uint8_t, uint8_t)> callback) {
    ioWriteCallback = callback;
}

void Z80A::triggerInterrupt() {
    if (!IFF1) return;
    
    if (halted) {
        halted = false;
    }

    IFF1 = IFF2 = false;
    
    // Z80 Interrupt: Pushes High then Low
    SP -= 2;
    memoryWriteCallback(SP + 1, (PC >> 8) & 0xFF);
    memoryWriteCallback(SP, PC & 0xFF);
    
    // For now, always use IM1 (jump to 0x0038)
    // TODO: Support other interrupt modes
    PC = 0x0038;
    total_cycles += 13;
}
