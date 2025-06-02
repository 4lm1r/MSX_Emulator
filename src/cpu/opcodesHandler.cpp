#include "opcodesHandler.h"
#include "cpu/z80a.h"
#include <fstream>
#include <unordered_map>
#include "debug.h"

OpcodesHandler::OpcodesHandler(Z80A& cpu) : cpu(cpu), cycles(0) {
    if (!debug_log.is_open()) {
        debug_log.open("debug.log", std::ios::out | std::ios::trunc);
    }
}

void OpcodesHandler::executeOpcode(uint8_t opcode) {
    cycles = 0; // Reset cycles for this instruction
    debug_log << "Executing opcode 0x" << std::hex << (int)opcode << " at PC=0x" << (int)cpu.PC
              << ", A=0x" << (int)cpu.A << ", SP=0x" << (int)cpu.SP << std::dec << std::endl;

   // Handle prefixes and special cases first
        if (opcode == 0xED) { // ED prefix for extended instructions (like RETI)
            uint8_t next_opcode = readMemory(cpu.PC + 1);
            debug_log << "ED prefix detected, next opcode: 0x" << std::hex << (int)next_opcode << std::dec << std::endl;
            cpu.PC++; // Consume the ED prefix
            if (next_opcode == 0x4D) { // RETI
                uint16_t addr = (readMemory(cpu.SP + 1) << 8) | readMemory(cpu.SP);
                cpu.SP += 2;
                cpu.PC = addr;
                debug_log << "RETI to 0x" << std::hex << addr << std::dec << std::endl;
                cycles = 14;
            } else {
                debug_log << "Unhandled ED instruction opcode 0x" << std::hex << (int)next_opcode << std::dec << std::endl;
                cpu.PC++;
                cycles = 4;
            }
        }
        else if (opcode == 0xDD) { // DD prefix (IX register instructions)
            uint8_t next_opcode = readMemory(cpu.PC + 1);
            debug_log << "DD prefix detected, next opcode: 0x" << std::hex << (int)next_opcode << std::dec << std::endl;
            cpu.PC++; // Consume the DD prefix
            handleIXInstructions(next_opcode);
        }
        else if (opcode == 0xCB) { // CB prefix (bit instructions)
            debug_log << "Routing to bit instruction block for opcode 0xCB" << std::endl;
            uint8_t nextByte = readMemory(cpu.PC + 1); // Fetch the next byte
            cpu.PC += 2; // Increment PC past the prefix and the instruction
            handleBitInstructions(nextByte);
            cycles = 8; // Base cycle count for CB instructions
        }
        else if (opcode == 0xFD) { // FD prefix (IY register instructions)
            uint8_t next_opcode = readMemory(cpu.PC + 1);
            debug_log << "FD prefix detected, next opcode: 0x" << std::hex << (int)next_opcode << std::dec << std::endl;
            cpu.PC++; // Consume the FD prefix
            handleIYInstructions(next_opcode);
        }
        // Explicit special cases
        else if (opcode == 0xD3 || opcode == 0xDB) { // OUT (n), A; IN A, (n)
            handleIO(opcode);
        }
        else if (opcode == 0xFE) { // CP n
            handleArithmetic(opcode);
        }
        else if (opcode == 0x00) { // NOP
            cpu.PC++;
            debug_log << "NOP" << std::endl;
            cycles = 4;
        }
        else if (opcode == 0x76) { // HALT
            cpu.halted = true;
            cpu.PC++;
            debug_log << "HALT" << std::endl;
            cycles = 4;
        }
        else if (opcode == 0x11) { // LD DE, nn
            uint16_t value = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.setDE(value);
            cpu.PC += 3;
            debug_log << "LD DE, 0x" << std::hex << value << std::dec << std::endl;
            cycles = 10;
        }
        // Specific opcode checks (e.g., LD instructions) before pattern-based blocks
        else if (opcode == 0x06 || opcode == 0x0E || opcode == 0x21 || opcode == 0x77 || opcode == 0x3E ||
                 opcode == 0x7E || opcode == 0x0A || opcode == 0x02 || opcode == 0x47 || opcode == 0x01 || 
                 opcode == 0x4D || opcode == 0x61 || opcode == 0x3A || opcode == 0x56 || opcode == 0x4F || 
                 opcode == 0x36 || opcode == 0x2E || opcode == 0x32) { // LD instructions
            debug_log << "Routing to handleDataTransfer" << std::endl;
            handleDataTransfer(opcode);
        }
        // Arithmetic and logic with immediate operands
        else if ((opcode & 0xCF) == 0xC6 || (opcode & 0xCF) == 0xCE) { // e.g., ADD A, n; ADC A, n
            if (opcode == 0xC6 || opcode == 0xD6) {
                debug_log << "Routing to handleArithmetic for immediate operand" << std::endl;
                handleArithmetic(opcode);
            } else {
                debug_log << "Routing to handleLogic for immediate operand" << std::endl;
                handleLogic(opcode);
            }
        }
        // Bit manipulation (RLCA, RRCA)
        else if (opcode == 0x07 || opcode == 0x0F) {
            debug_log << "Routing to handleBitManip" << std::endl;
            handleBitManip(opcode);
        }
        // Control flow and stack (11xx xxxx)
        else if ((opcode & 0xC0) == 0xC0) {
            debug_log << "Routing to control flow/stack block" << std::endl;
            if (opcode == 0xC5 || opcode == 0xC1 || opcode == 0xD1 || opcode == 0xE5 || 
                opcode == 0xE1 || opcode == 0xF1 || opcode == 0xE3 || opcode == 0xD5 || opcode == 0xF5) {
                debug_log << "Routing to handleStack" << std::endl;
                handleStack(opcode);
            } else if (opcode == 0xD9 || opcode == 0x08) { // EXX or EX AF, AF'
                debug_log << "Routing to handleControlFlow" << std::endl;
                handleControlFlow(opcode);
            } else {
                debug_log << "Routing to handleControlFlow" << std::endl;
                handleControlFlow(opcode);
            }
        }
        // Arithmetic and logic (10xx xxxx)
        else if ((opcode & 0xC0) == 0x80) {
            debug_log << "Routing to arithmetic/logic block for opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            if (opcode == 0xA0 || opcode == 0xA2 || opcode == 0xAF || opcode == 0xB0 || 
                opcode == 0xA8) { 
                debug_log << "Routing to handleLogic" << std::endl;
                handleLogic(opcode);
            } else if (opcode == 0x80 || opcode == 0x83 || opcode == 0x86 || opcode == 0x90 || 
                       opcode == 0x7C || opcode == 0x93 || opcode == 0x9A || opcode == 0xBE) {
                debug_log << "Routing to handleArithmetic" << std::endl;
                handleArithmetic(opcode);
            } else {
                debug_log << "Routing to handleArithmetic" << std::endl;
                handleArithmetic(opcode);
            }
        }
        // Misc block (00xx xxxx) - moved lower to avoid capturing specific opcodes like 0x3E
        else if ((opcode & 0xC0) == 0x00) {
            debug_log << "Routing to misc block for opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            if (opcode == 0x2F) { // CPL
                debug_log << "Routing to handleLogic" << std::endl;
                handleLogic(opcode);
            } else {
                debug_log << "Unhandled in misc block, opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
                cpu.PC++;
                cycles = 4;
            }
        }
        // Data transfer, arithmetic, control flow (remaining 00xx xxxx and 01xx xxxx)
        else {
            debug_log << "Routing to default block for opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            if (opcode == 0x3C || opcode == 0x3D || opcode == 0x80 || opcode == 0x90 || 
                opcode == 0x86 || opcode == 0x04 || opcode == 0x0C || opcode == 0x05 || 
                opcode == 0x23 || opcode == 0x03 || opcode == 0x24 || opcode == 0x93 || 
                opcode == 0x9A || opcode == 0xBE || opcode == 0x2C || opcode == 0x25) {
                debug_log << "Routing to handleArithmetic" << std::endl;
                handleArithmetic(opcode);
            }
            else if (opcode == 0x20 || opcode == 0x28 || opcode == 0x38 || opcode == 0x10 || opcode == 0x30) {
                debug_log << "Routing to handleControlFlow" << std::endl;
                handleControlFlow(opcode);
            }
            else {
                debug_log << "Unhandled opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
                cpu.PC++;
                cycles = 4;
            }
        }

        debug_log << "After execution, PC=0x" << std::hex << (int)cpu.PC
                  << ", A=0x" << (int)cpu.A << ", SP=0x" << (int)cpu.SP
                  << ", F=0x" << (int)cpu.F << ", cycles=" << cycles << std::dec << std::endl;
}

void OpcodesHandler::InitializeOpcodeMap() {
		handleIY[ 0xE5 ] = &OpcodesHandler::IY_OXE5;
		handleIY[ 0x77 ] = &OpcodesHandler::IY_OX77;
		handleIY[ 0x7E ] = &OpcodesHandler::IY_OX7E;
		dataTransfer[ 0x3E ] = &OpcodesHandler::DT_OX3E;
		dataTransfer[ 0x06 ] = &OpcodesHandler::DT_OXO6;
		dataTransfer[ 0x0E ] = &OpcodesHandler::DT_OXOE;
		dataTransfer[ 0x01 ] = &OpcodesHandler::DT_OXO1;
		dataTransfer[ 0x21 ] = &OpcodesHandler::DT_OX21;
		dataTransfer[ 0x77 ] = &OpcodesHandler::DT_OX77;
		dataTransfer[ 0x7E ] = &OpcodesHandler::DT_OX7E;
	}
// New IY instruction handler (similar to IX)

void OpcodesHandler::handleIYInstructions(uint8_t opcode) {
	if (handleIY.empty()) {
        InitializeOpcodeMap();
    }

    auto it = handleIY.find(opcode);
    if (it != handleIY.end()) {
        (this->*(it->second))(opcode); // Call the mapped function
    } else {
        debug_log << "Unhandled arithmetic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
	
}
void OpcodesHandler::handleIXInstructions(uint8_t opcode) {
    if (handleIX.empty()) {
        InitializeOpcodeMap();
    }

    auto it = handleIX.find(opcode);
    if (it != handleIX.end()) {
        (this->*(it->second))(opcode); // Call the mapped function
    } else {
        debug_log << "Unhandled arithmetic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }

}
void OpcodesHandler::handleDataTransfer(uint8_t opcode) {
         if (dataTransfer.empty()) {
        InitializeOpcodeMap();
    }

    auto it = dataTransfer.find(opcode);
    if (it != dataTransfer.end()) {
        (this->*(it->second))(opcode); // Call the mapped function
    } else {
        debug_log << "Unhandled arithmetic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }      
}
	void OpcodesHandler::IY_OXE5(uint8_t) {
		cpu.SP -= 2;
		writeMemory(cpu.SP + 1, (cpu.IY >> 8) & 0xFF);
		writeMemory(cpu.SP, cpu.IY & 0xFF);
		cpu.PC++;
		debug_log << "PUSH IY" << std::endl;
		cycles = 15;
	}
	void OpcodesHandler::IY_OX77(uint8_t) {  // LD (IY+d), A
		int8_t d = readMemory(cpu.PC + 1);
		uint16_t addr = cpu.IY + d;
		writeMemory(addr, cpu.A);
		cpu.PC += 2;
		debug_log << "LD (IY+d=0x" << std::hex << addr << "), A (0x" << (int)cpu.A << ")" << std::dec << std::endl;
		cycles = 19;
	}
	void OpcodesHandler::IY_OX7E(uint8_t) {
		int8_t d = readMemory(cpu.PC + 1);
		uint16_t addr = cpu.IY + d;
		cpu.A = readMemory(addr);
		cpu.PC += 2;
		debug_log << "LD A, (IY+d=0x" << std::hex << addr << ") = 0x" << (int)cpu.A << std::dec << std::endl;
		cycles = 19;
	}
	void OpcodesHandler::IX_OX77(uint8_t) { // LD (IX+d), A
            int8_t d = readMemory(cpu.PC + 1);
            uint16_t addr = cpu.IX + d;
            writeMemory(addr, cpu.A);
            cpu.PC += 2;
            debug_log << "LD (IX+d=0x" << std::hex << addr << "), A (0x" << (int)cpu.A << ")" << std::dec << std::endl;
            cycles = 19;
        }
        void OpcodesHandler::IX_OX74(uint8_t) { // LD (IX+d), H
            int8_t d = readMemory(cpu.PC + 1);
            uint16_t addr = cpu.IX + d;
            writeMemory(addr, cpu.H);
            cpu.PC += 2;
            debug_log << "LD (IX+d=0x" << std::hex << addr << "), H (0x" << (int)cpu.H << ")" << std::dec << std::endl;
            cycles = 19;
        }
        void OpcodesHandler::IX_OX75(uint8_t) { // LD (IX+d), L
            int8_t d = readMemory(cpu.PC + 1);
            uint16_t addr = cpu.IX + d;
            writeMemory(addr, cpu.L);
            cpu.PC += 2;
            debug_log << "LD (IX+d=0x" << std::hex << addr << "), L (0x" << (int)cpu.L << ")" << std::dec << std::endl;
            cycles = 19;
        }
        void OpcodesHandler::IX_OX7E(uint8_t) { // LD A, (IX+d)
            int8_t d = readMemory(cpu.PC + 1);
            uint16_t addr = cpu.IX + d;
            cpu.A = readMemory(addr);
            cpu.PC += 2;
            debug_log << "LD A, (IX+d=0x" << std::hex << addr << ") = 0x" << (int)cpu.A << std::dec << std::endl;
            cycles = 19;
        }
        void OpcodesHandler::IX_OXE5(uint8_t) { // PUSH IX
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, (cpu.IX >> 8) & 0xFF);
            writeMemory(cpu.SP, cpu.IX & 0xFF);
            cpu.PC++;
            debug_log << "PUSH IX" << std::endl;
            cycles = 15;
        }
	void OpcodesHandler::DT_OX3E(uint8_t) {   // LD A, n
	      uint8_t value = readMemory(cpu.PC + 1);
	      cpu.A = value;
	      cpu.PC += 2;
	      debug_log << "LD A, 0x" << std::hex << (int)value << std::dec << std::endl;
	      cycles = 7;
	}
	void OpcodesHandler::DT_OXO6(uint8_t) { // LD B, n
            uint8_t value = readMemory(cpu.PC + 1);
            cpu.B = value;
            cpu.PC += 2;
            debug_log << "LD B, 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OXOE(uint8_t) { // LD C, n
            uint8_t value = readMemory(cpu.PC + 1);
            cpu.C = value;
            cpu.PC += 2;
            debug_log << "LD C, 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OXO1(uint8_t) { // LD BC, nn
            uint16_t value = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.setBC(value);
            cpu.PC += 3;
            debug_log << "LD BC, 0x" << std::hex << value << std::dec << std::endl;
            cycles = 10;
        }
        void OpcodesHandler::DT_OX21(uint8_t) { // LD HL, nn
            uint16_t value = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.setHL(value);
            cpu.PC += 3;
            debug_log << "LD HL, 0x" << std::hex << value << std::dec << std::endl;
            cycles = 10;
        }
        void OpcodesHandler::DT_OX77(uint8_t) { // LD (HL), A
            uint16_t addr = cpu.getHL();
            writeMemory(addr, cpu.A);
            cpu.PC++;
            debug_log << "LD (HL=0x" << std::hex << addr << "), A (0x" << (int)cpu.A << ")" << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OX7E(uint8_t) { // LD A, (HL)
            uint16_t addr = cpu.getHL();
            cpu.A = readMemory(addr);
            cpu.PC++;
            debug_log << "LD A, (HL=0x" << std::hex << addr << ") = 0x" << (int)cpu.A << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OXOA(uint8_t) { // LD A, (BC)
            uint16_t addr = cpu.getBC();
            cpu.A = readMemory(addr);
            cpu.PC++;
            debug_log << "LD A, (BC=0x" << std::hex << addr << ") = 0x" << (int)cpu.A << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OXO2(uint8_t) { // LD (BC), A
            uint16_t addr = cpu.getBC();
            writeMemory(addr, cpu.A);
            cpu.PC++;
            debug_log << "LD (BC=0x" << std::hex << addr << "), A (0x" << (int)cpu.A << ")" << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OX47(uint8_t) { // LD B, A
            cpu.B = cpu.A;
            cpu.PC++;
            debug_log << "LD B, A" << std::endl;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX4D(uint8_t) { // LD C, L
            cpu.C = cpu.L;
            cpu.PC++;
            debug_log << "LD C, L" << std::endl;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX61(uint8_t) { // LD H, C
            cpu.H = cpu.C;
            cpu.PC++;
            debug_log << "LD H, C" << std::endl;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX3A(uint8_t) { // LD A, (nn)
            uint16_t addr = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.A = readMemory(addr);
            cpu.PC += 3;
            debug_log << "LD A, (0x" << std::hex << addr << ") = 0x" << (int)cpu.A << std::dec << std::endl;
            cycles = 13;
        }
        void OpcodesHandler::DT_OX56(uint8_t) { // LD D, (HL)
            uint16_t addr = cpu.getHL();
            cpu.D = readMemory(addr);
            cpu.PC++;
            debug_log << "LD D, (HL=0x" << std::hex << addr << ") = 0x" << (int)cpu.D << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OX4F(uint8_t) { // LD C, A
            cpu.C = cpu.A;
            debug_log << "LD C, A: C=0x" << std::hex << (int)cpu.C << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX36(uint8_t) { // LD (HL), n
            uint8_t value = readMemory(cpu.PC + 1);
            uint16_t addr = cpu.getHL();
            writeMemory(addr, value);
            cpu.PC += 2;
            debug_log << "LD (HL=0x" << std::hex << addr << "), 0x" << (int)value << std::dec << std::endl;
            cycles = 10;
        }
        void OpcodesHandler::DT_OX2E(uint8_t) { // LD L, n
            uint8_t value = readMemory(cpu.PC + 1);
            cpu.L = value;
            cpu.PC += 2;
            debug_log << "LD L, 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
        }
        void OpcodesHandler::DT_OX32(uint8_t) { // LD (nn), A
            uint16_t addr = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            writeMemory(addr, cpu.A);
            cpu.PC += 3;
            debug_log << "LD (0x" << std::hex << addr << "), A (0x" << (int)cpu.A << ")" << std::dec << std::endl;
            cycles = 13;
        }
        void OpcodesHandler::DT_OX7D(uint8_t) { // LD A, L
            cpu.A = cpu.L;
            cpu.PC++;
            debug_log << "LD A, L" << std::endl;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX78(uint8_t) { // LD A, B
            cpu.A = cpu.B;
            cpu.PC++;
            debug_log << "LD A, B" << std::endl;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX67(uint8_t) { // LD H, A
            cpu.H = cpu.A;
            cpu.PC++;
            debug_log << "LD H, A" << std::endl;
            cycles = 4;
        }
        void OpcodesHandler::DT_OX6F(uint8_t) { // LD L, A
            cpu.L = cpu.A;
            cpu.PC++;
            debug_log << "LD L, A" << std::endl;
            cycles = 4;
        }



void OpcodesHandler::handleArithmetic(uint8_t opcode) {
    switch (opcode) {
        case 0x80: { // ADD A, B
            uint16_t result = cpu.A + cpu.B;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result > 0xFF) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC++;
            debug_log << "ADD A, B" << std::endl;
            cycles = 4;
            break;
        }
        case 0x83: { // ADD A, E
            uint16_t result = cpu.A + cpu.E;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result > 0xFF) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC++;
            debug_log << "ADD A, E" << std::endl;
            cycles = 4;
            break;
        }
        case 0x86: { // ADD A, (HL)
            uint16_t addr = cpu.getHL();
            uint8_t value = readMemory(addr);
            uint16_t result = cpu.A + value;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result > 0xFF) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC++;
            debug_log << "ADD A, (HL=0x" << std::hex << addr << ") = 0x" << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0x90: { // SUB B
            int16_t result = cpu.A - cpu.B;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result < 0) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC++;
            debug_log << "SUB B" << std::endl;
            cycles = 4;
            break;
        }
        case 0xD6: { // SUB n
            uint8_t value = readMemory(cpu.PC + 1);
            int16_t result = cpu.A - value;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result < 0) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC += 2;
            debug_log << "SUB 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0xC6: { // ADD A, n
            uint8_t value = readMemory(cpu.PC + 1);
            uint16_t result = cpu.A + value;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result > 0xFF) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC += 2;
            debug_log << "ADD A, 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0x3C: { // INC A
            cpu.A++;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "INC A" << std::endl;
            cycles = 4;
            break;
        }
        case 0x3D: { // DEC A
            cpu.A--;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "DEC A" << std::endl;
            cycles = 4;
            break;
        }
        case 0x04: { // INC B
            cpu.B++;
            cpu.F = (cpu.B == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "INC B" << std::endl;
            cycles = 4;
            break;
        }
        case 0x0C: { // INC C
            cpu.C++;
            cpu.F = (cpu.C == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "INC C" << std::endl;
            cycles = 4;
            break;
        }
        case 0x05: { // DEC B
            cpu.B--;
            cpu.F = (cpu.B == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "DEC B" << std::endl;
            cycles = 4;
            break;
        }
        case 0x23: { // INC HL
            uint16_t hl = cpu.getHL();
            hl++;
            cpu.setHL(hl);
            cpu.PC++;
            debug_log << "INC HL" << std::endl;
            cycles = 6;
            break;
        }
        case 0x03: { // INC BC
            uint16_t bc = cpu.getBC();
            bc++;
            cpu.setBC(bc);
            cpu.PC++;
            debug_log << "INC BC" << std::endl;
            cycles = 6;
            break;
        }
        case 0x24: { // INC H
            cpu.H++;
            cpu.F = (cpu.H == 0) ? 0x80 : 0; // Zero flag
            cpu.PC++;
            debug_log << "INC H" << std::endl;
            cycles = 4;
            break;
        }
        case 0xFE: { // CP n
            uint8_t value = readMemory(cpu.PC + 1);
            int16_t result = cpu.A - value;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result < 0) ? 0x10 : 0;
            cpu.PC += 2;
            debug_log << "CP 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0x93: { // SUB E
            int16_t result = cpu.A - cpu.E;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result < 0) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC++;
            debug_log << "SUB E" << std::endl;
            cycles = 4;
            break;
        }
        case 0x7C: { // LD A, H
            cpu.A = cpu.H;
            cpu.PC++;
            debug_log << "LD A, H" << std::endl;
            cycles = 4;
            break;
        }
        case 0x9A: { // SBC A, D
            int16_t result = cpu.A - cpu.D - ((cpu.F & 0x10) ? 1 : 0);
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result < 0) ? 0x10 : 0;
            cpu.A = result & 0xFF;
            cpu.PC++;
            debug_log << "SBC A, D" << std::endl;
            cycles = 4;
            break;
        }
        case 0xBE: { // CP (HL)
            uint16_t addr = cpu.getHL();
            uint8_t value = readMemory(addr);
            int16_t result = cpu.A - value;
            cpu.F = (result & 0xFF) == 0 ? 0x80 : 0;
            cpu.F |= (result < 0) ? 0x10 : 0;
            cpu.PC++;
            debug_log << "CP (HL=0x" << std::hex << addr << ") = 0x" << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0x2C: { // INC L
            uint8_t old_L = cpu.L;
            cpu.L++;
            cpu.F = (cpu.F & 0x10); // Preserve Carry flag
            cpu.F |= (cpu.L == 0) ? 0x80 : 0; // Zero flag
            cpu.F |= (cpu.L & 0x80) ? 0x40 : 0; // Sign flag
            cpu.F |= ((old_L & 0x0F) == 0x0F) ? 0x20 : 0; // Half-Carry flag
            cpu.F &= ~0x02; // Reset N flag
            cpu.PC++;
            debug_log << "INC L" << std::endl;
            cycles = 4;
            break;
        }
        case 0x25: { // DEC H
            uint8_t old_H = cpu.H;
            cpu.H--;
            cpu.F = (cpu.F & 0x10); // Preserve Carry flag
            cpu.F |= (cpu.H == 0) ? 0x80 : 0; // Zero flag
            cpu.F |= (cpu.H & 0x80) ? 0x40 : 0; // Sign flag
            cpu.F |= ((old_H & 0x0F) == 0x00) ? 0x20 : 0; // Half-Carry flag
            cpu.F |= 0x02; // Set N flag
            cpu.PC++;
            debug_log << "DEC H" << std::endl;
            cycles = 4;
            break;
        }
        default:
            debug_log << "Unhandled arithmetic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
}

void OpcodesHandler::handleLogic(uint8_t opcode) {
    switch (opcode) {
        case 0xA0: { // AND B
            cpu.A &= cpu.B;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "AND B" << std::endl;
            cycles = 4;
            break;
        }
        case 0xA2: { // AND D
            cpu.A &= cpu.D;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "AND D" << std::endl;
            cycles = 4;
            break;
        }
        case 0xAF: { // XOR A
            cpu.A ^= cpu.A;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "XOR A" << std::endl;
            cycles = 4;
            break;
        }
        case 0xB0: { // OR B
            cpu.A |= cpu.B;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "OR B" << std::endl;
            cycles = 4;
            break;
        }
        case 0xE6: { // AND n
            uint8_t value = readMemory(cpu.PC + 1);
            cpu.A &= value;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC += 2;
            debug_log << "AND 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0xEE: { // XOR n
            uint8_t value = readMemory(cpu.PC + 1);
            cpu.A ^= value;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC += 2;
            debug_log << "XOR 0x" << std::hex << (int)value << std::dec << std::endl;
            cycles = 7;
            break;
        }
        case 0xA8: { // XOR B
            cpu.A ^= cpu.B;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "XOR B" << std::endl;
            cycles = 4;
            break;
        }
        case 0x2F: { // CPL
            cpu.A = ~cpu.A;
            cpu.F |= 0x10; // Set N flag
            cpu.F |= 0x20; // Set H flag
            cpu.PC++;
            debug_log << "CPL" << std::endl;
            cycles = 4;
            break;
        }
        case 0xA7: { // AND A
            cpu.A &= cpu.A;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.PC++;
            debug_log << "AND A" << std::endl;
            cycles = 4;
            break;
        }
        default:
            debug_log << "Unhandled logic opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
}

void OpcodesHandler::handleControlFlow(uint8_t opcode) {
    switch (opcode) {
        case 0xC3: { // JP nn
            uint16_t addr = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.PC = addr;
            debug_log << "JP 0x" << std::hex << addr << std::dec << std::endl;
            cycles = 10;
            break;
        }
        case 0xCD: { // CALL
            uint16_t addr = (readMemory(cpu.PC + 2) << 8) | readMemory(cpu.PC + 1);
            cpu.last_call_return = cpu.PC + 3;
            writeMemory(--cpu.SP, (cpu.PC + 3) & 0xFF);
            writeMemory(--cpu.SP, (cpu.PC + 3) >> 8);
            cpu.PC = addr;
            cycles = 17;
            break;
        }
        case 0xC9: { // RET
            uint16_t addr = (readMemory(cpu.SP + 1) << 8) | readMemory(cpu.SP);
            if (cpu.SP < 0xfffc || addr == 0x0000) {
                addr = cpu.last_call_return;
                debug_log << "RET: Fallback to last call return address" << std::endl;
            }
            cpu.SP += 2;
            debug_log << "RET: SP=0x" << std::hex << (int)(cpu.SP - 2)
                      << ", Stack[SP]=0x" << (int)readMemory(cpu.SP - 2)
                      << " 0x" << (int)readMemory(cpu.SP - 1) << std::dec << std::endl;
            debug_log << "RET to 0x" << std::hex << addr << ", SP=0x" << (int)cpu.SP << std::dec
                      << std::endl;
            cpu.PC = addr;
            cycles = 10;
            break;
        }
        case 0xF0: { // RET P
            bool sign = (cpu.F & 0x80) == 0;
            if (sign) {
                uint16_t addr = (readMemory(cpu.SP + 1) << 8) | readMemory(cpu.SP);
                cpu.SP += 2;
                cpu.PC = addr;
                debug_log << "RET P to 0x" << std::hex << addr << std::dec << std::endl;
                cycles = 11;
            } else {
                cpu.PC++;
                debug_log << "RET P skipped (sign flag set)" << std::endl;
                cycles = 5;
            }
            break;
        }
        case 0xCF: { // RST 08H
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, (cpu.PC >> 8) & 0xFF);
            writeMemory(cpu.SP, cpu.PC & 0xFF);
            cpu.PC = 0x0008;
            debug_log << "RST 08H" << std::endl;
            cycles = 11;
            break;
        }
        case 0xFF: { // RST 38H
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, (cpu.PC >> 8) & 0xFF);
            writeMemory(cpu.SP, cpu.PC & 0xFF);
            cpu.PC = 0x0038;
            cpu.interruptPending = false;
            debug_log << "RST 38H" << std::endl;
            cycles = 11;
            break;
        }
        case 0xF3: { // DI
            debug_log << "DI (Interrupts disabled)" << std::endl;
            cycles = 4;
            break;
        }
        case 0xFB: { // EI
            debug_log << "EI (Interrupts enabled)" << std::endl;
            cycles = 4;
            break;
        }
        case 0x20: { // JR NZ, e
            int8_t offset = readMemory(cpu.PC + 1);
            if (!(cpu.F & 0x80)) {
                cpu.PC += offset + 2;
                debug_log << "JR NZ, 0x" << std::hex << (int)offset << std::dec << std::endl;
                cycles = 12;
            } else {
                cpu.PC += 2;
                debug_log << "JR NZ skipped (Z flag set)" << std::endl;
                cycles = 7;
            }
            break;
        }
        case 0x28: { // JR Z, e
            int8_t offset = readMemory(cpu.PC + 1);
            if (cpu.F & 0x80) {
                cpu.PC += offset + 2;
                debug_log << "JR Z, 0x" << std::hex << (int)offset << std::dec << std::endl;
                cycles = 12;
            } else {
                cpu.PC += 2;
                debug_log << "JR Z skipped (Z flag not set)" << std::endl;
                cycles = 7;
            }
            break;
        }
        case 0x38: { // JR C, e
            int8_t offset = readMemory(cpu.PC + 1);
            if (cpu.F & 0x10) {
                cpu.PC += offset + 2;
                debug_log << "JR C, 0x" << std::hex << (int)offset << std::dec << std::endl;
                cycles = 12;
            } else {
                cpu.PC += 2;
                debug_log << "JR C skipped (C flag not set)" << std::endl;
                cycles = 7;
            }
            break;
        }
        case 0x10: { // DJNZ e
            int8_t offset = readMemory(cpu.PC + 1);
            cpu.B--;
            if (cpu.B != 0) {
                cpu.PC += offset + 2;
                debug_log << "DJNZ 0x" << std::hex << (int)offset << std::dec << std::endl;
                cycles = 13;
            } else {
                cpu.PC += 2;
                debug_log << "DJNZ skipped (B=0)" << std::endl;
                cycles = 8;
            }
            break;
        }
        case 0xD9: { // EXX
            std::swap(cpu.B, cpu.B_);
            std::swap(cpu.C, cpu.C_);
            std::swap(cpu.D, cpu.D_);
            std::swap(cpu.E, cpu.E_);
            std::swap(cpu.H, cpu.H_);
            std::swap(cpu.L, cpu.L_);
            cpu.PC++;
            debug_log << "EXX" << std::endl;
            cycles = 4;
            break;
        }
        case 0x08: { // EX AF, AF'
            uint8_t tempA = cpu.A;
            uint8_t tempF = cpu.F;
            cpu.A = cpu.A_;
            cpu.F = cpu.F_;
            cpu.A_ = tempA;
            cpu.F_ = tempF;
            cpu.PC++;
            debug_log << "EX AF, AF'" << std::endl;
            cycles = 4;
            break;
        }
        case 0xDD: { // IX prefix
            uint8_t next = readMemory(cpu.PC + 1);
            if (next == 0xE5) { // PUSH IX
                cpu.SP -= 2;
                writeMemory(cpu.SP + 1, (cpu.IX >> 8) & 0xFF);
                writeMemory(cpu.SP, cpu.IX & 0xFF);
                debug_log << "PUSH IX" << std::endl;
                cpu.PC += 2;
                cycles = 15;
            }
            break;
        }
        case 0xF6: { // OR n
            uint8_t n = readMemory(cpu.PC + 1);
            cpu.A |= n;
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            if (cpu.A & 0x08) cpu.F |= 0x20;
            debug_log << "OR " << std::hex << (int)n << std::dec << std::endl;
            cpu.PC += 2;
            cycles = 7;
            break;
        }
        case 0x30: { // JR NC, e
            int8_t offset = readMemory(cpu.PC + 1);
            if (!(cpu.F & 0x10)) {
                cpu.PC += offset + 2;
                debug_log << "JR NC, 0x" << std::hex << (int)offset << std::dec << std::endl;
                cycles = 12;
            } else {
                cpu.PC += 2;
                debug_log << "JR NC skipped (C flag set)" << std::endl;
                cycles = 7;
            }
            break;
        }
        case 0xEB: { // EX DE, HL
            uint8_t tempD = cpu.D;
            uint8_t tempE = cpu.E;
            cpu.D = cpu.H;
            cpu.E = cpu.L;
            cpu.H = tempD;
            cpu.L = tempE;
            cpu.PC++;
            debug_log << "EX DE, HL" << std::endl;
            cycles = 4;
            break;
        }
        default:
            debug_log << "Unhandled control flow opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
}

void OpcodesHandler::handleIO(uint8_t opcode) {
    switch (opcode) {
        case 0xD3: { // OUT (n), A
            uint8_t port = readMemory(cpu.PC + 1);
            writeIO(port, cpu.A);
            cpu.PC += 2;
            debug_log << "OUT (0x" << std::hex << (int)port << "), A (0x" << (int)cpu.A << ")" << std::dec << std::endl;
            cycles = 11;
            break;
        }
        case 0xDB: { // IN A, (n)
            uint8_t port = readMemory(cpu.PC + 1);
            cpu.A = readIO(port);
            cpu.PC += 2;
            debug_log << "IN A, (0x" << std::hex << (int)port << ") = 0x" << (int)cpu.A << std::dec << std::endl;
            cycles = 11;
            break;
        }
        default:
            debug_log << "Unhandled I/O opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
}

void OpcodesHandler::handleBitManip(uint8_t opcode) {
    switch (opcode) {
        case 0x07: { // RLCA
            uint8_t carry = (cpu.A & 0x80) ? 0x10 : 0;
            cpu.A = (cpu.A << 1) | (carry ? 1 : 0);
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.F |= carry;
            cpu.PC++;
            debug_log << "RLCA" << std::endl;
            cycles = 4;
            break;
        }
        case 0x0F: { // RRCA
            uint8_t carry = (cpu.A & 0x01) ? 0x10 : 0;
            cpu.A = (cpu.A >> 1) | (carry ? 0x80 : 0);
            cpu.F = (cpu.A == 0) ? 0x80 : 0;
            cpu.F |= carry;
            cpu.PC++;
            debug_log << "RRCA" << std::endl;
            cycles = 4;
            break;
        }
        default:
            debug_log << "Unhandled bit manipulation opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
}

void OpcodesHandler::handleStack(uint8_t opcode) {
    switch (opcode) {
        case 0xC5: { // PUSH BC
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, cpu.B);
            writeMemory(cpu.SP, cpu.C);
            cpu.PC++;
            debug_log << "PUSH BC" << std::endl;
            cycles = 11;
            break;
        }
        case 0xC1: { // POP BC
            cpu.C = readMemory(cpu.SP);
            cpu.B = readMemory(cpu.SP + 1);
            cpu.SP += 2;
            cpu.PC++;
            debug_log << "POP BC" << std::endl;
            cycles = 10;
            break;
        }
        case 0xD1: { // POP DE
            cpu.E = readMemory(cpu.SP);
            cpu.D = readMemory(cpu.SP + 1);
            cpu.SP += 2;
            cpu.PC++;
            debug_log << "POP DE" << std::endl;
            cycles = 10;
            break;
        }
        case 0xE5: { // PUSH HL
            uint16_t hl = cpu.getHL();
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, (hl >> 8) & 0xFF);
            writeMemory(cpu.SP, hl & 0xFF);
            cpu.PC++;
            debug_log << "PUSH HL" << std::endl;
            cycles = 11;
            break;
        }
        case 0xE1: { // POP HL
            uint8_t l = readMemory(cpu.SP);
            uint8_t h = readMemory(cpu.SP + 1);
            cpu.setHL((h << 8) | l);
            cpu.SP += 2;
            cpu.PC++;
            debug_log << "POP HL" << std::endl;
            cycles = 10;
            break;
        }
        case 0xF1: { // POP AF
            cpu.F = readMemory(cpu.SP);
            cpu.A = readMemory(cpu.SP + 1);
            cpu.SP += 2;
            cpu.PC++;
            debug_log << "POP AF, SP=0x" << std::hex << cpu.SP << ", read F=0x" << (int)cpu.F << ", A=0x" << (int)cpu.A << std::dec << std::endl;
            cycles = 10;
            break;
        }
        case 0xE3: { // EX (SP), HL
            uint16_t sp_addr = cpu.SP;
            uint8_t sp_h = cpu.memoryReadCallback(sp_addr + 1);
            uint8_t sp_l = cpu.memoryReadCallback(sp_addr);
            uint16_t hl = cpu.getHL();
            uint8_t h = (hl >> 8) & 0xFF;
            uint8_t l = hl & 0xFF;
            cpu.memoryWriteCallback(sp_addr + 1, h);
            cpu.memoryWriteCallback(sp_addr, l);
            cpu.setHL((sp_h << 8) | sp_l);
            debug_log << "EX (SP), HL: HL=0x" << std::hex << cpu.getHL() << std::dec << std::endl;
            cycles = 19;
            cpu.PC++;
            break;
        }
        case 0xD5: { // PUSH DE
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, cpu.D);
            writeMemory(cpu.SP, cpu.E);
            cpu.PC++;
            debug_log << "PUSH DE" << std::endl;
            cycles = 11;
            break;
        }
        case 0xF5: { // PUSH AF
            cpu.SP -= 2;
            writeMemory(cpu.SP + 1, cpu.A);
            writeMemory(cpu.SP, cpu.F);
            cpu.PC++;
            debug_log << "PUSH AF" << std::endl;
            cycles = 11;
            break;
        }
        case 0xF9: { // LD SP, HL
            cpu.SP = cpu.getHL();
            cpu.PC++;
            debug_log << "LD SP, HL (0x" << std::hex << cpu.SP << ")" << std::dec << std::endl;
            cycles = 6;
            break;
        }
        default:
            debug_log << "Unhandled stack opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            cpu.PC++;
            cycles = 4;
    }
}

int OpcodesHandler::handleBitInstructions(uint8_t opcode) {
    int cycles = 8; // Base cycle count for most CB instructions (register-based)

    // Decode the instruction type (BIT, SET, RES) and target bit/register
    uint8_t operation = (opcode >> 6) & 0x03; // Top 2 bits: 00 = RLC/RRC/SLA/etc., 01 = BIT, 10 = RES, 11 = SET
    uint8_t bit = (opcode >> 3) & 0x07;       // Middle 3 bits: bit position (0-7)
    uint8_t reg = opcode & 0x07;              // Bottom 3 bits: register (0=B, 1=C, ..., 5=L, 6=(HL), 7=A)

    // Helper to get the target register value (or (HL) from memory)
    uint8_t* target = nullptr;
    bool isMemory = false;
    switch (reg) {
        case 0: target = &cpu.B; break;
        case 1: target = &cpu.C; break;
        case 2: target = &cpu.D; break;
        case 3: target = &cpu.E; break;
        case 4: target = &cpu.H; break;
        case 5: target = &cpu.L; break;
        case 6: isMemory = true; break; // (HL)
        case 7: target = &cpu.A; break;
    }

    uint16_t hl = (cpu.H << 8) | cpu.L;
    uint8_t value = isMemory ? readMemory(hl) : *target;

    switch (operation) {
        case 0x01: { // BIT b, r
            bool bitSet = (value >> bit) & 0x01;
            cpu.F = (cpu.F & 0x01) | 0x20; // Preserve C flag, set H flag, clear N flag
            if (!bitSet) cpu.F |= 0x40;    // Set Z flag if bit is 0
            if (bit == 7 && bitSet) cpu.F |= 0x80; // Set S flag if bit 7 is 1
            cpu.F |= 0x10; // Set P/V flag same as Z (parity not used in BIT)
            debug_log << "BIT " << (int)bit << ", " << (isMemory ? "(HL)" : reg == 7 ? "A" : reg == 0 ? "B" : reg == 1 ? "C" : reg == 2 ? "D" : reg == 3 ? "E" : reg == 4 ? "H" : "L") << std::endl;
            if (isMemory) cycles = 12; // Extra cycles for memory access
            break;
        }
        case 0x02: { // RES b, r
            value &= ~(1 << bit); // Clear the bit
            if (isMemory) writeMemory(hl, value);
            else *target = value;
            debug_log << "RES " << (int)bit << ", " << (isMemory ? "(HL)" : reg == 7 ? "A" : reg == 0 ? "B" : reg == 1 ? "C" : reg == 2 ? "D" : reg == 3 ? "E" : reg == 4 ? "H" : "L") << std::endl;
            if (isMemory) cycles = 15; // Extra cycles for memory access
            break;
        }
        case 0x03: { // SET b, r
            value |= (1 << bit); // Set the bit
            if (isMemory) writeMemory(hl, value);
            else *target = value;
            debug_log << "SET " << (int)bit << ", " << (isMemory ? "(HL)" : reg == 7 ? "A" : reg == 0 ? "B" : reg == 1 ? "C" : reg == 2 ? "D" : reg == 3 ? "E" : reg == 4 ? "H" : "L") << std::endl;
            if (isMemory) cycles = 15; // Extra cycles for memory access
            break;
        }
        default: { // Handle other CB instructions (e.g., shifts like RLC, RRC, SLA)
            debug_log << "Unhandled CB instruction: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            break;
        }
    }

    return cycles;
}

uint8_t OpcodesHandler::readMemory(uint16_t addr) {
    debug_log << "Reading memory at 0x" << std::hex << addr << std::dec << std::endl;
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

int OpcodesHandler::getCycles() const {
    return cycles;
}
