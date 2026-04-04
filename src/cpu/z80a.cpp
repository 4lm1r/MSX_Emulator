#include "z80a.h"
#include "opcodesHandler.h"
#include "debug.h"
#include <iomanip>
#include <iostream>

Z80A::Z80A() : A(0), B(0), C(0), D(0), E(0), H(0), L(0), F(0), PC(0), SP(0xF380),
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
    SP = 0xF380; 
    halted = false;
    IFF1 = IFF2 = false;
    interruptPending = false;
    total_cycles = 0;
    EI_pending = false;
    std::cout << "Z80A::reset: PC=0x" << std::hex << PC << " SP=0x" << SP << std::dec << std::endl;
}

int Z80A::execute() {
    if (halted) {
        static int halt_count = 0;
        if (halt_count < 1) {
            std::cout << "CPU halted at PC=0x" << std::hex << PC << std::dec << std::endl;
            halt_count++;
        }
        return 4;
    }

    // Handle EI pending: interrupts are enabled after the instruction following EI
    bool old_EI_pending = EI_pending;
    EI_pending = false;
    
    uint8_t opcode = memoryReadCallback(PC);
    
    // Always trace first 30 instructions
    static int instruction_count = 0;
    if (instruction_count < 30) {
        std::cout << "CPU TRACE PC=0x" << std::hex << std::setw(4) << std::setfill('0') << PC 
                  << " op=0x" << std::setw(2) << std::setfill('0') << (int)opcode;
        // Show next two bytes for context
        uint8_t next1 = memoryReadCallback(PC + 1);
        uint8_t next2 = memoryReadCallback(PC + 2);
        std::cout << " next=0x" << std::setw(2) << (int)next1 
                  << " 0x" << std::setw(2) << (int)next2;
        std::cout << " A=0x" << std::setw(2) << (int)A 
                  << " BC=0x" << std::setw(4) << getBC()
                  << " DE=0x" << std::setw(4) << getDE()
                  << " HL=0x" << std::setw(4) << getHL()
                  << " SP=0x" << std::setw(4) << SP;
        std::cout << std::dec << std::endl;
        instruction_count++;
    }
    
    debug_log << "PC: 0x" << std::hex << PC << " Op: 0x" << (int)opcode << std::dec << std::endl;
    PC++;

    opcodeHandler->executeOpcode(opcode);
    int cycles = opcodeHandler->getCycles();
    total_cycles += cycles;
    
    // If EI was executed in the previous instruction, enable interrupts now
    if (old_EI_pending) {
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
        // Don't increment PC - it's pointing to the HALT instruction
        // which will be executed again after RETI unless we handle it
    }

    IFF1 = IFF2 = false;
    
    // Push current PC onto stack
    SP -= 2;
    memoryWriteCallback(SP + 1, (PC >> 8) & 0xFF);
    memoryWriteCallback(SP, PC & 0xFF);
    
    // For now, always use IM1 (jump to 0x0038)
    // TODO: Support other interrupt modes
    PC = 0x0038;
    total_cycles += 11;
}
