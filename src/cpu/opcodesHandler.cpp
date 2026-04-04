#include "opcodesHandler.h"
#include "cpu/z80a.h"
#include <fstream>
#include <unordered_map>
#include <iomanip>
#include <iostream>
#include "debug.h"

OpcodesHandler::OpcodesHandler(Z80A& cpu) : cpu(cpu), cycles(0) {
    if (!debug_log.is_open()) {
        debug_log.open("debug.log", std::ios::out | std::ios::trunc);
    }
}

uint16_t OpcodesHandler::read16(uint16_t addr) {
  return readMemory(addr) | (readMemory(addr + 1) << 8);
}

void OpcodesHandler::updateSZP(uint8_t result) {
  cpu.F &= ~(Z80A::S_BIT | Z80A::Z_BIT | Z80A::P_BIT); // Clear S, Z, P 
  if (result & 0x80) cpu.F |= Z80A::S_BIT;
  if (result == 0) cpu.F |= Z80A::Z_BIT;

  // Parity calculation (bit count is even)
  uint8_t p = result;
  p ^= p >> 4;
  p ^= p >> 2;
  p ^= p >> 1;
  if (!(p & 1)) cpu.F |= Z80A::P_BIT;
}

void OpcodesHandler::updateFlagsAdd(uint8_t a, uint8_t b, uint16_t result) {
  uint8_t res8 = (uint8_t)result;
  cpu.F = 0;
  if (res8 & 0x80) cpu.F |= Z80A::S_BIT;
  if (res8 == 0) cpu.F |= Z80A::Z_BIT;
  if ((a & 0x0F) + (b & 0x0F) > 0x0F) cpu.F |= Z80A::H_BIT;
  // Overflow: set if two positives made a negative or two negatives made a positive 
  if (((a ^ res8) & (b ^ res8) & 0x80)) cpu.F |= Z80A::P_BIT;
  if (result > 0xFF) cpu.F |= Z80A::C_BIT;
  // N_BIT is 0 for addition 
}

void OpcodesHandler::updateFlagsSub(uint8_t a, uint8_t b, uint16_t result) {
  uint8_t res8 = (uint8_t)result;
  cpu.F = Z80A::N_BIT; // Set N flag because this is a subtraction 

  if (res8 & 0x80) cpu.F |= Z80A::S_BIT;
  if (res8 == 0) cpu.F |= Z80A::Z_BIT;
  // Half-carry for subtraction: borrow from bit 4 
  if ((a & 0x0F) < (b & 0x0F)) cpu.F |= Z80A::H_BIT;
  // Overflow: set if operands had different signs and result sign is different from 'a'
  if (((a ^ b) & (a ^ res8) & 0x80)) cpu.F |= Z80A::P_BIT;
  if (result > 0xFF) cpu.F |= Z80A::C_BIT;
}

void OpcodesHandler::updateFlagsAdc(uint8_t a, uint8_t b, uint16_t result) {
    uint8_t res8 = (uint8_t)result;
    uint8_t carry = (cpu.F & Z80A::C_BIT) ? 1 : 0;
    cpu.F = 0;
    if (res8 & 0x80) cpu.F |= Z80A::S_BIT;
    if (res8 == 0) cpu.F |= Z80A::Z_BIT;
    if ((a & 0x0F) + (b & 0x0F) + carry > 0x0F) cpu.F |= Z80A::H_BIT;
    if (((a ^ res8) & (b ^ res8) & 0x80)) cpu.F |= Z80A::P_BIT;
    if (result > 0xFF) cpu.F |= Z80A::C_BIT;
}

void OpcodesHandler::updateFlagsSbc(uint8_t a, uint8_t b, uint16_t result) {
    uint8_t res8 = (uint8_t)result;
    uint8_t carry = (cpu.F & Z80A::C_BIT) ? 1 : 0;
    cpu.F = Z80A::N_BIT;
    if (res8 & 0x80) cpu.F |= Z80A::S_BIT;
    if (res8 == 0) cpu.F |= Z80A::Z_BIT;
    if ((a & 0x0F) < (b & 0x0F) + carry) cpu.F |= Z80A::H_BIT;
    if (((a ^ b) & (a ^ res8) & 0x80)) cpu.F |= Z80A::P_BIT;
    if (result > 0xFF) cpu.F |= Z80A::C_BIT;
}

int OpcodesHandler::handleCB(uint8_t opcode) {
    uint8_t regIndex = opcode & 0x07;
    uint8_t bit = (opcode >> 3) & 0x07;
    uint8_t category = (opcode >> 6) & 0x03;

    auto getReg = [&](uint8_t index) -> uint8_t {
        switch(index) {
            case 0: return cpu.B; case 1: return cpu.C; case 2: return cpu.D;
            case 3: return cpu.E; case 4: return cpu.H; case 5: return cpu.L;
            case 6: return readMemory(cpu.getHL());
            default: return cpu.A;
        }
    };

    auto setReg = [&](uint8_t index, uint8_t val) {
        switch(index) {
            case 0: cpu.B = val; break; case 1: cpu.C = val; break;
            case 2: cpu.D = val; break; case 3: cpu.E = val; break;
            case 4: cpu.H = val; break; case 5: cpu.L = val; break;
            case 6: writeMemory(cpu.getHL(), val); break;
            default: cpu.A = val; break;
        }
    };

    if (category == 1) { // BIT n, r
        uint8_t val = getReg(regIndex);
        cpu.F &= ~(Z80A::N_BIT | Z80A::Z_BIT);
        cpu.F |= Z80A::H_BIT;
        if (!(val & (1 << bit))) cpu.F |= Z80A::Z_BIT;
        // Sign flag is set if bit 7 is tested and is 1
        if (bit == 7 && (val & 0x80)) cpu.F |= Z80A::S_BIT; else cpu.F &= ~Z80A::S_BIT;
        return (regIndex == 6) ? 12 : 8;
    } 
    else if (category == 2) { // RES n, r
        setReg(regIndex, getReg(regIndex) & ~(1 << bit));
        return (regIndex == 6) ? 15 : 8;
    } 
    else if (category == 3) { // SET n, r
        setReg(regIndex, getReg(regIndex) | (1 << bit));
        return (regIndex == 6) ? 15 : 8;
    }
    else { // Category 0: Shifts and Rotates
        uint8_t val = getReg(regIndex);
        uint8_t opType = (opcode >> 3) & 0x07;
        uint8_t old_carry = (cpu.F & Z80A::C_BIT);

        switch(opType) {
            case 0: { // RLC (Rotate Left Circular)
                uint8_t c = val >> 7;
                val = (val << 1) | c;
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
            case 1: { // RRC (Rotate Right Circular)
                uint8_t c = val & 0x01;
                val = (val >> 1) | (c << 7);
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
            case 2: { // RL (Rotate Left through Carry)
                uint8_t c = val >> 7;
                val = (val << 1) | (old_carry ? 1 : 0);
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
            case 3: { // RR (Rotate Right through Carry)
                uint8_t c = val & 0x01;
                val = (val >> 1) | (old_carry ? 0x80 : 0);
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
            case 4: { // SLA (Shift Left Arithmetic)
                uint8_t c = val >> 7;
                val <<= 1;
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
            case 5: { // SRA (Shift Right Arithmetic)
                uint8_t c = val & 0x01;
                val = (val & 0x80) | (val >> 1); // Keep bit 7
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
            case 7: { // SRL (Shift Right Logical)
                uint8_t c = val & 0x01;
                val >>= 1;
                cpu.F = c ? Z80A::C_BIT : 0;
                break;
            }
        }
        updateSZP(val); // Standard update for S, Z, P flags after shifts
        cpu.F &= ~(Z80A::H_BIT | Z80A::N_BIT); // H and N always cleared
        setReg(regIndex, val);
        return (regIndex == 6) ? 15 : 8;
    }
}

int OpcodesHandler::handleED(uint8_t opcode) {
    switch (opcode) {
        case 0x4D: { // RETI (Return from Interrupt)
            cpu.PC = read16(cpu.SP);
            cpu.SP += 2;
            return 14;
        }
        case 0x56: return 8; // IM 1 (Common for MSX)
        case 0xB0: { // LDIR
            writeMemory(cpu.getDE(), readMemory(cpu.getHL()));
            cpu.setHL(cpu.getHL() + 1);
            cpu.setDE(cpu.getDE() + 1);
            cpu.setBC(cpu.getBC() - 1);
            if (cpu.getBC() != 0) {
                cpu.PC -= 2;
                return 21;
            }
            return 16;
        }
        case 0x73: { // LD (nn), SP
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            writeMemory(addr, cpu.SP & 0xFF);
            writeMemory(addr + 1, cpu.SP >> 8);
            return 20;
        }
        case 0x7B: { // LD SP, (nn)
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            cpu.SP = read16(addr);
            return 20;
        }
        default:
            debug_log << "Unimplemented ED Opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 8;
    }
}

int OpcodesHandler::handleIX(uint8_t opcode) {
    switch (opcode) {
        case 0x21: { // LD IX, nn
            cpu.IX = read16(cpu.PC);
            cpu.PC += 2;
            return 14;
        }
        case 0x2A: { // LD IX, (nn)
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            cpu.IX = read16(addr);
            return 20;
        }
        case 0x36: { // LD (IX+d), n
            int8_t d = (int8_t)readMemory(cpu.PC++);
            uint8_t n = readMemory(cpu.PC++);
            writeMemory(cpu.IX + d, n);
            return 19;
        }
        case 0x7E: { // LD A, (IX+d)
            int8_t d = (int8_t)readMemory(cpu.PC++);
            cpu.A = readMemory(cpu.IX + d);
            return 19;
        }
        case 0x19: { // ADD IX, DE
            uint32_t res = (uint32_t)cpu.IX + cpu.getDE();
            cpu.F &= ~(Z80A::N_BIT | Z80A::H_BIT | Z80A::C_BIT);
            if (((cpu.IX & 0x0FFF) + (cpu.getDE() & 0x0FFF)) & 0x1000) cpu.F |= Z80A::H_BIT;
            if (res & 0x10000) cpu.F |= Z80A::C_BIT;
            cpu.IX = res & 0xFFFF;
            return 15;
        }
        case 0xE1: { // POP IX
            cpu.IX = readMemory(cpu.SP) | (readMemory(cpu.SP + 1) << 8);
            cpu.SP += 2;
            return 14;
        }
        case 0xE5: { // PUSH IX
            cpu.SP -= 2;
            writeMemory(cpu.SP, cpu.IX & 0xFF);
            writeMemory(cpu.SP + 1, cpu.IX >> 8);
            return 15;
        }
        case 0xE9: { // JP (IX)
            cpu.PC = cpu.IX;
            return 8;
        }
        default:
            debug_log << "Unimplemented IX Opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

int OpcodesHandler::handleIY(uint8_t opcode) {
    switch (opcode) {
        case 0x21: { // LD IY, nn
            cpu.IY = read16(cpu.PC);
            cpu.PC += 2;
            return 14;
        }
        case 0x2A: { // LD IY, (nn)
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            cpu.IY = read16(addr);
            return 20;
        }
        case 0x7E: { // LD A, (IY+d)
            int8_t d = (int8_t)readMemory(cpu.PC++);
            cpu.A = readMemory(cpu.IY + d);
            return 19;
        }
        case 0xE1: { // POP IY
            cpu.IY = readMemory(cpu.SP) | (readMemory(cpu.SP + 1) << 8);
            cpu.SP += 2;
            return 14;
        }
        case 0xE5: { // PUSH IY
            cpu.SP -= 2;
            writeMemory(cpu.SP, cpu.IY & 0xFF);
            writeMemory(cpu.SP + 1, cpu.IY >> 8);
            return 15;
        }
        default:
            debug_log << "Unimplemented IY Opcode: 0x" << std::hex << (int)opcode << std::endl;
            return 4;
    }
}

void OpcodesHandler::executeOpcode(uint8_t opcode) {
    cycles = 4; // Default

    switch (opcode) {
        case 0x00: cycles = 4; break; // NOP
        
        case 0xCB: { // Prefix CB
            uint8_t cb_opcode = readMemory(cpu.PC++);
            cycles = handleCB(cb_opcode);
            break;
        }
        
        case 0xED: { // Prefix ED
            uint8_t ed_opcode = readMemory(cpu.PC++);
            cycles = handleED(ed_opcode);
            break;
        }

        case 0xDD: { // Prefix DD (IX)
            uint8_t ix_opcode = readMemory(cpu.PC++);
            cycles = handleIX(ix_opcode);
            break;
        }

        case 0xFD: { // Prefix FD (IY)
            uint8_t iy_opcode = readMemory(cpu.PC++);
            cycles = handleIY(iy_opcode);
            break;
        }

        case 0x08: { // EX AF, AF'
            uint8_t temp_a = cpu.A;
            uint8_t temp_f = cpu.F;
            cpu.A = cpu.A_;
            cpu.F = cpu.F_;
            cpu.A_ = temp_a;
            cpu.F_ = temp_f;
            cycles = 4;
            break;
        }

        case 0x10: { // DJNZ e
            int8_t offset = (int8_t)readMemory(cpu.PC++);
            cpu.B--;
            if (cpu.B != 0) {
                cpu.PC += offset;
                cycles = 13;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0xEB: { // EX DE, HL
            uint16_t de = cpu.getDE();
            uint16_t hl = cpu.getHL();
            cpu.setDE(hl);
            cpu.setHL(de);
            cycles = 4;
            break;
        }

        case 0xD9: { // EXX
            uint16_t b_temp = cpu.B, c_temp = cpu.C, d_temp = cpu.D, e_temp = cpu.E, h_temp = cpu.H, l_temp = cpu.L;
            cpu.B = cpu.B_; cpu.C = cpu.C_; cpu.D = cpu.D_; cpu.E = cpu.E_; cpu.H = cpu.H_; cpu.L = cpu.L_;
            cpu.B_ = b_temp; cpu.C_ = c_temp; cpu.D_ = d_temp; cpu.E_ = e_temp; cpu.H_ = h_temp; cpu.L_ = l_temp;
            cycles = 4;
            break;
        }

        // RST Instructions
        case 0xC7: // RST 00h
        case 0xCF: // RST 08h
        case 0xD7: // RST 10h
        case 0xDF: // RST 18h
        case 0xE7: // RST 20h
        case 0xEF: // RST 28h
        case 0xF7: // RST 30h
        case 0xFF: // RST 38h
        {
            cpu.SP -= 2;
            writeMemory(cpu.SP, cpu.PC & 0xFF);
            writeMemory(cpu.SP + 1, cpu.PC >> 8);
            cpu.PC = opcode & 0x38;
            cycles = 11;
            break;
        }

        case 0x07: { // RLCA
            uint8_t c = cpu.A >> 7;
            cpu.A = (cpu.A << 1) | c;
            cpu.F &= ~(Z80A::H_BIT | Z80A::N_BIT | Z80A::C_BIT);
            if (c) cpu.F |= Z80A::C_BIT;
            cycles = 4;
            break;
        }

        case 0x0F: { // RRCA
            uint8_t c = cpu.A & 0x01;
            cpu.A = (cpu.A >> 1) | (c << 7);
            cpu.F &= ~(Z80A::H_BIT | Z80A::N_BIT | Z80A::C_BIT);
            if (c) cpu.F |= Z80A::C_BIT;
            cycles = 4;
            break;
        }

        case 0x17: { // RLA
            uint8_t c = cpu.A >> 7;
            cpu.A = (cpu.A << 1) | ((cpu.F & Z80A::C_BIT) ? 1 : 0);
            cpu.F &= ~(Z80A::H_BIT | Z80A::N_BIT | Z80A::C_BIT);
            if (c) cpu.F |= Z80A::C_BIT;
            cycles = 4;
            break;
        }

        case 0x1F: { // RRA
            uint8_t c = cpu.A & 0x01;
            cpu.A = (cpu.A >> 1) | ((cpu.F & Z80A::C_BIT) ? 0x80 : 0);
            cpu.F &= ~(Z80A::H_BIT | Z80A::N_BIT | Z80A::C_BIT);
            if (c) cpu.F |= Z80A::C_BIT;
            cycles = 4;
            break;
        }

        case 0x3E: { // LD A, n
            cpu.A = readMemory(cpu.PC++);
            cycles = 7;
            break;
        }

        case 0xF9: { // LD SP, HL
          cpu.SP = cpu.getHL();
          cycles = 6;
          break;
        }

        case 0x02: { // LD (BC), A
            writeMemory(cpu.getBC(), cpu.A);
            cycles = 7;
            break;
        }
        case 0x12: { // LD (DE), A
            writeMemory(cpu.getDE(), cpu.A);
            cycles = 7;
            break;
        }
        case 0x0A: { // LD A, (BC)
            cpu.A = readMemory(cpu.getBC());
            cycles = 7;
            break;
        }
        case 0x1A: { // LD A, (DE)
            cpu.A = readMemory(cpu.getDE());
            cycles = 7;
            break;
        }

        case 0xC2: { // JP NZ, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (!(cpu.F & Z80A::Z_BIT)) cpu.PC = addr;
            cycles = 10;
            break;
        }
        case 0xCA: { // JP Z, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (cpu.F & Z80A::Z_BIT) cpu.PC = addr;
            cycles = 10;
            break;
        }
        case 0xD2: { // JP NC, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (!(cpu.F & Z80A::C_BIT)) cpu.PC = addr;
            cycles = 10;
            break;
        }
        case 0xDA: { // JP C, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (cpu.F & Z80A::C_BIT) cpu.PC = addr;
            cycles = 10;
            break;
        }
        case 0xF2: { // JP P, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (!(cpu.F & Z80A::S_BIT)) cpu.PC = addr;
            cycles = 10;
            break;
        }
        case 0xFA: { // JP M, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (cpu.F & Z80A::S_BIT) cpu.PC = addr;
            cycles = 10;
            break;
        }

        case 0xC4: { // CALL NZ, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (!(cpu.F & Z80A::Z_BIT)) {
                cpu.SP -= 2;
                writeMemory(cpu.SP, cpu.PC & 0xFF);
                writeMemory(cpu.SP + 1, cpu.PC >> 8);
                cpu.PC = addr;
                cycles = 17;
            } else {
                cycles = 10;
            }
            break;
        }
        case 0xDC: { // CALL C, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (cpu.F & Z80A::C_BIT) {
                cpu.SP -= 2;
                writeMemory(cpu.SP, cpu.PC & 0xFF);
                writeMemory(cpu.SP + 1, cpu.PC >> 8);
                cpu.PC = addr;
                cycles = 17;
            } else {
                cycles = 10;
            }
            break;
        }
        case 0xFC: { // CALL M, nn
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            if (cpu.F & Z80A::S_BIT) {
                cpu.SP -= 2;
                writeMemory(cpu.SP, cpu.PC & 0xFF);
                writeMemory(cpu.SP + 1, cpu.PC >> 8);
                cpu.PC = addr;
                cycles = 17;
            } else {
                cycles = 10;
            }
            break;
        }

        case 0xC3: { // JP nn (Absolute Jump)
            cpu.PC = read16(cpu.PC);
            cycles = 10;
            break;
        }

        case 0x21: { // LD HL, nn
            cpu.setHL(read16(cpu.PC));
            cpu.PC += 2;
            cycles = 10;
            break;
        }

        case 0x18: { // JR e (Relative Jump)
            int8_t offset = (int8_t)readMemory(cpu.PC++);
            cpu.PC += offset;
            cycles = 12;
            break;
        }

        case 0x20: { // JR NZ, e
            int8_t offset = (int8_t)readMemory(cpu.PC++);
            if (!(cpu.F & Z80A::Z_BIT)) {
                cpu.PC += offset;
                cycles = 12;
            } else {
                cycles = 7;
            }
            break;
        }

        case 0x28: { // JR Z, e
          int8_t offset = (int8_t)readMemory(cpu.PC++);
          if (cpu.F & Z80A::Z_BIT) {
            cpu.PC += offset;
            cycles = 12;
          } else {
            cycles = 7;
          }
          break;
        }

        case 0x30: { // JR NC, e
            int8_t offset = (int8_t)readMemory(cpu.PC++);
            if (!(cpu.F & Z80A::C_BIT)) {
                cpu.PC += offset;
                cycles = 12;
            } else {
                cycles = 7;
            }
            break;
        }

        case 0x38: { // JR C, e
            int8_t offset = (int8_t)readMemory(cpu.PC++);
            if (cpu.F & Z80A::C_BIT) {
                cpu.PC += offset;
                cycles = 12;
            } else {
                cycles = 7;
            }
            break;
        }

        case 0x22: { // LD (nn), HL
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            writeMemory(addr, cpu.L);
            writeMemory(addr + 1, cpu.H);
            cycles = 16;
            break;
        }

        case 0x2A: { // LD HL, (nn)
            uint16_t addr = read16(cpu.PC);
            cpu.PC += 2;
            cpu.L = readMemory(addr);
            cpu.H = readMemory(addr + 1);
            cycles = 16;
            break;
        }

        case 0x2F: { // CPL (Complement A)
            cpu.A = ~cpu.A;
            cpu.F |= (Z80A::H_BIT | Z80A::N_BIT);
            cycles = 4;
            break;
        }

        case 0x36: { // LD (HL), n
            uint8_t n = readMemory(cpu.PC++);
            writeMemory(cpu.getHL(), n);
            cycles = 10;
            break;
        }

        case 0xCD: {  // CALL nn 
          uint16_t dest = read16(cpu.PC);
          cpu.PC += 2;
          std::cout << "CALL nn: dest=0x" << std::hex << dest 
                    << " current PC=0x" << cpu.PC 
                    << " SP before push=0x" << cpu.SP << std::dec << std::endl;
          // Push current PC onto stack
          cpu.SP -= 2;
          writeMemory(cpu.SP, cpu.PC & 0xFF);      // Low Byte 
          writeMemory(cpu.SP + 1, (cpu.PC >> 8));  // High Byte 
          std::cout << "CALL: pushed return address 0x" << std::hex << cpu.PC 
                    << " to SP=0x" << cpu.SP << std::dec << std::endl;
          cpu.PC = dest;
          cycles = 17;
          break;
        }

        case 0x31: { // LD SP, nn 
          uint16_t addr = read16(cpu.PC);
          cpu.SP = addr;
          cpu.PC += 2;
          std::cout << "LD SP, nn: SP set to 0x" << std::hex << cpu.SP 
                    << " (read from PC=0x" << (cpu.PC-2) << ")" << std::dec << std::endl;
          // Log if SP is in ROM area
          if (cpu.SP < 0x8000) {
              std::cout << "WARNING: SP in ROM area! (0x" << std::hex << cpu.SP << ")" << std::dec << std::endl;
          }
          cycles = 10;
          break;
        }

        case 0x01: {  // LD BC, nn 
          cpu.setBC(read16(cpu.PC));
          cpu.PC += 2;
          cycles = 10;
          break;
        }

        case 0x11: {  // LD DE, nn
          cpu.setDE(read16(cpu.PC));
          cpu.PC += 2;
          cycles = 10;
          break;
        }

        case 0xC5: {  // PUSH BC 
          cpu.SP -= 2;
          writeMemory(cpu.SP, cpu.C);
          writeMemory(cpu.SP + 1, cpu.B);
          // Verify the write only if in RAM area (0x8000-0xFFFF)
          if (cpu.SP >= 0x8000) {
              uint8_t low = readMemory(cpu.SP);
              uint8_t high = readMemory(cpu.SP + 1);
              if (low != cpu.C || high != cpu.B) {
                  std::cout << "PUSH BC: WARNING: write mismatch at SP=0x" << std::hex << cpu.SP 
                            << " wrote C=0x" << (int)cpu.C << " read=0x" << (int)low
                            << " wrote B=0x" << (int)cpu.B << " read=0x" << (int)high << std::dec << std::endl;
              }
          }
          cycles = 11;
          break;
        }

        case 0xC1: {   // POP BC 
          cpu.C = readMemory(cpu.SP);
          cpu.B = readMemory(cpu.SP + 1);
          cpu.SP += 2;
          cycles = 10;
          break;
        }

        case 0xD5: {  // PUSH DE
          cpu.SP -= 2;
          writeMemory(cpu.SP, cpu.E);
          writeMemory(cpu.SP + 1, cpu.D);
          cycles = 11;
          break;
        }

        case 0xD1: {  // POP DE
          cpu.E = readMemory(cpu.SP);
          cpu.D = readMemory(cpu.SP + 1);
          cpu.SP += 2;
          cycles = 10;
          break;
        }

        case 0xE5: {    // PUSH HL 
          cpu.SP -= 2;
          writeMemory(cpu.SP, cpu.L);
          writeMemory(cpu.SP + 1, cpu.H);
          cycles = 11;
          break;
        }

        case 0xE1: {  // POP HL 
          cpu.L = readMemory(cpu.SP);
          cpu.H = readMemory(cpu.SP + 1);
          cpu.SP += 2;
          cycles = 10;
          break;
        }

        case 0xE3: { // EX (SP), HL
          uint8_t low = readMemory(cpu.SP);
          uint8_t high = readMemory(cpu.SP + 1);
          writeMemory(cpu.SP, cpu.L);
          writeMemory(cpu.SP + 1, cpu.H);
          cpu.L = low;
          cpu.H = high;
          cycles = 19;
          break;
        }

        case 0xF5: { // PUSH AF
           cpu.SP -= 2;
           writeMemory(cpu.SP, cpu.F);      // Low Byte is Flags
           writeMemory(cpu.SP + 1, cpu.A);  // High Byte is Accumulator
           // Verify the write only if in RAM area (0x8000-0xFFFF)
           if (cpu.SP >= 0x8000) {
               uint8_t low = readMemory(cpu.SP);
               uint8_t high = readMemory(cpu.SP + 1);
               if (low != cpu.F || high != cpu.A) {
                   std::cout << "PUSH AF: WARNING: write mismatch at SP=0x" << std::hex << cpu.SP 
                             << " wrote F=0x" << (int)cpu.F << " read=0x" << (int)low
                             << " wrote A=0x" << (int)cpu.A << " read=0x" << (int)high << std::dec << std::endl;
               }
           }
           cycles = 11;
           break;
        }

        case 0xF1: { // POP AF
           cpu.F = readMemory(cpu.SP);      // Low Byte restores Flags
           cpu.A = readMemory(cpu.SP + 1);  // High Byte restores Accumulator
           cpu.SP += 2;
           cycles = 10;
           break;
        }

        case 0x06: {  // LD B, n 
          cpu.B = readMemory(cpu.PC++);
          cycles = 7;
          break;
        }

        case 0x0E: {   // LD C, n 
          cpu.C = readMemory(cpu.PC++);
          cycles = 7;
          break;
        }

        case 0x16: {   // LD D, n 
          cpu.D = readMemory(cpu.PC++);
          cycles = 7;
          break;
        }

        case 0x1E: {   // LD E, n 
          cpu.E = readMemory(cpu.PC++);
          cycles = 7;
          break;
        }

        case 0x26: {   // LD H, n 
           cpu.H = readMemory(cpu.PC++);
           cycles = 7;
           break;
        }

        case 0x2E: {   // LD L, n 
           cpu.L = readMemory(cpu.PC++);
           cycles = 7;
           break;
        }

        case 0x3A: {  // LD A, (nn)
           uint16_t addr = read16(cpu.PC);
           cpu.PC += 2;
           cpu.A = readMemory(addr);
           cycles = 13;
           break;
        }

        case 0x32: {  // LD (nn), A 
           uint16_t addr = read16(cpu.PC);
           cpu.PC += 2;
           writeMemory(addr, cpu.A);
           cycles = 13;
           break;
        }

        // --- LD r, r and LD r, (HL) ---
        // B Row
        case 0x40: { cpu.B = cpu.B; break; }
        case 0x41: { cpu.B = cpu.C; break; }
        case 0x42: { cpu.B = cpu.D; break; }
        case 0x43: { cpu.B = cpu.E; break; }
        case 0x44: { cpu.B = cpu.H; break; }
        case 0x45: { cpu.B = cpu.L; break; }
        case 0x46: { cpu.B = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x47: { cpu.B = cpu.A; break; }
        // C Row
        case 0x48: { cpu.C = cpu.B; break; }
        case 0x49: { cpu.C = cpu.C; break; }
        case 0x4A: { cpu.C = cpu.D; break; }
        case 0x4B: { cpu.C = cpu.E; break; }
        case 0x4C: { cpu.C = cpu.H; break; }
        case 0x4D: { cpu.C = cpu.L; break; }
        case 0x4E: { cpu.C = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x4F: { cpu.C = cpu.A; break; }
        // D Row
        case 0x50: { cpu.D = cpu.B; break; }
        case 0x51: { cpu.D = cpu.C; break; }
        case 0x52: { cpu.D = cpu.D; break; }
        case 0x53: { cpu.D = cpu.E; break; }
        case 0x54: { cpu.D = cpu.H; break; }
        case 0x55: { cpu.D = cpu.L; break; }
        case 0x56: { cpu.D = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x57: { cpu.D = cpu.A; break; }
        // E Row
        case 0x58: { cpu.E = cpu.B; break; }
        case 0x59: { cpu.E = cpu.C; break; }
        case 0x5A: { cpu.E = cpu.D; break; }
        case 0x5B: { cpu.E = cpu.E; break; }
        case 0x5C: { cpu.E = cpu.H; break; }
        case 0x5D: { cpu.E = cpu.L; break; }
        case 0x5E: { cpu.E = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x5F: { cpu.E = cpu.A; break; }
        // H Row
        case 0x60: { cpu.H = cpu.B; break; }
        case 0x61: { cpu.H = cpu.C; break; }
        case 0x62: { cpu.H = cpu.D; break; }
        case 0x63: { cpu.H = cpu.E; break; }
        case 0x64: { cpu.H = cpu.H; break; }
        case 0x65: { cpu.H = cpu.L; break; }
        case 0x66: { cpu.H = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x67: { cpu.H = cpu.A; break; }
        // L Row
        case 0x68: { cpu.L = cpu.B; break; }
        case 0x69: { cpu.L = cpu.C; break; }
        case 0x6A: { cpu.L = cpu.D; break; }
        case 0x6B: { cpu.L = cpu.E; break; }
        case 0x6C: { cpu.L = cpu.H; break; }
        case 0x6D: { cpu.L = cpu.L; break; }
        case 0x6E: { cpu.L = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x6F: { cpu.L = cpu.A; break; }
        // (HL) Row
        case 0x70: { writeMemory(cpu.getHL(), cpu.B); cycles = 7; break; }
        case 0x71: { writeMemory(cpu.getHL(), cpu.C); cycles = 7; break; }
        case 0x72: { writeMemory(cpu.getHL(), cpu.D); cycles = 7; break; }
        case 0x73: { writeMemory(cpu.getHL(), cpu.E); cycles = 7; break; }
        case 0x74: { writeMemory(cpu.getHL(), cpu.H); cycles = 7; break; }
        case 0x75: { writeMemory(cpu.getHL(), cpu.L); cycles = 7; break; }
        case 0x76: { cpu.halted = true; cycles = 4; break; } // HALT
        case 0x77: { writeMemory(cpu.getHL(), cpu.A); cycles = 7; break; }
        // A Row
        case 0x78: { cpu.A = cpu.B; break; }
        case 0x79: { cpu.A = cpu.C; break; }
        case 0x7A: { cpu.A = cpu.D; break; }
        case 0x7B: { cpu.A = cpu.E; break; }
        case 0x7C: { cpu.A = cpu.H; break; }
        case 0x7D: { cpu.A = cpu.L; break; }
        case 0x7E: { cpu.A = readMemory(cpu.getHL()); cycles = 7; break; }
        case 0x7F: { cpu.A = cpu.A; break; }

        // --- Arithmetic Group ---
        case 0x80: { // ADD A, B
            uint16_t res = (uint16_t)cpu.A + cpu.B;
            updateFlagsAdd(cpu.A, cpu.B, res);
            cpu.A = (uint8_t)res;
            break;
        }
        case 0x81: { // ADD A, C
            uint16_t res = (uint16_t)cpu.A + cpu.C;
            updateFlagsAdd(cpu.A, cpu.C, res);
            cpu.A = (uint8_t)res;
            break;
        }
        case 0x82: { // ADD A, D
            uint16_t res = (uint16_t)cpu.A + cpu.D;
            updateFlagsAdd(cpu.A, cpu.D, res);
            cpu.A = (uint8_t)res;
            break;
        }
        case 0x83: { uint16_t res = (uint16_t)cpu.A + cpu.E; updateFlagsAdd(cpu.A, cpu.E, res); cpu.A = (uint8_t)res; break; }
        case 0x84: { uint16_t res = (uint16_t)cpu.A + cpu.H; updateFlagsAdd(cpu.A, cpu.H, res); cpu.A = (uint8_t)res; break; }
        case 0x85: { uint16_t res = (uint16_t)cpu.A + cpu.L; updateFlagsAdd(cpu.A, cpu.L, res); cpu.A = (uint8_t)res; break; }
        case 0x86: { uint8_t val = readMemory(cpu.getHL()); uint16_t res = (uint16_t)cpu.A + val; updateFlagsAdd(cpu.A, val, res); cpu.A = (uint8_t)res; cycles = 7; break; }
        case 0x87: { uint16_t res = (uint16_t)cpu.A + cpu.A; updateFlagsAdd(cpu.A, cpu.A, res); cpu.A = (uint8_t)res; break; }

        case 0x88: { // ADC A, B
            uint16_t res = (uint16_t)cpu.A + cpu.B + ((cpu.F & Z80A::C_BIT) ? 1 : 0);
            updateFlagsAdc(cpu.A, cpu.B, res);
            cpu.A = (uint8_t)res;
            break;
        }
        case 0x89: { uint16_t res = (uint16_t)cpu.A + cpu.C + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, cpu.C, res); cpu.A = (uint8_t)res; break; }
        case 0x8A: { uint16_t res = (uint16_t)cpu.A + cpu.D + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, cpu.D, res); cpu.A = (uint8_t)res; break; }
        case 0x8B: { uint16_t res = (uint16_t)cpu.A + cpu.E + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, cpu.E, res); cpu.A = (uint8_t)res; break; }
        case 0x8C: { uint16_t res = (uint16_t)cpu.A + cpu.H + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, cpu.H, res); cpu.A = (uint8_t)res; break; }
        case 0x8D: { uint16_t res = (uint16_t)cpu.A + cpu.L + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, cpu.L, res); cpu.A = (uint8_t)res; break; }
        case 0x8E: { uint8_t val = readMemory(cpu.getHL()); uint16_t res = (uint16_t)cpu.A + val + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, val, res); cpu.A = (uint8_t)res; cycles = 7; break; }
        case 0x8F: { uint16_t res = (uint16_t)cpu.A + cpu.A + ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsAdc(cpu.A, cpu.A, res); cpu.A = (uint8_t)res; break; }
        case 0xCE: { // ADC A, n
            uint8_t n = readMemory(cpu.PC++);
            uint16_t res = (uint16_t)cpu.A + n + ((cpu.F & Z80A::C_BIT) ? 1 : 0);
            updateFlagsAdc(cpu.A, n, res);
            cpu.A = (uint8_t)res;
            cycles = 7;
            break;
        }

        case 0xC6: { // ADD A, n
            uint8_t n = readMemory(cpu.PC++);
            uint16_t result = cpu.A + n;
            updateFlagsAdd(cpu.A, n, result);
            cpu.A = (uint8_t)result;
            cycles = 7;
            break;
        }

        case 0x90: { // SUB B
            uint16_t res = (uint16_t)cpu.A - cpu.B;
            updateFlagsSub(cpu.A, cpu.B, res);
            cpu.A = (uint8_t)res;
            break;
        }
        case 0x91: { uint16_t res = (uint16_t)cpu.A - cpu.C; updateFlagsSub(cpu.A, cpu.C, res); cpu.A = (uint8_t)res; break; }
        case 0x92: { uint16_t res = (uint16_t)cpu.A - cpu.D; updateFlagsSub(cpu.A, cpu.D, res); cpu.A = (uint8_t)res; break; }
        case 0x93: { uint16_t res = (uint16_t)cpu.A - cpu.E; updateFlagsSub(cpu.A, cpu.E, res); cpu.A = (uint8_t)res; break; }
        case 0x94: { uint16_t res = (uint16_t)cpu.A - cpu.H; updateFlagsSub(cpu.A, cpu.H, res); cpu.A = (uint8_t)res; break; }
        case 0x95: { uint16_t res = (uint16_t)cpu.A - cpu.L; updateFlagsSub(cpu.A, cpu.L, res); cpu.A = (uint8_t)res; break; }
        case 0x96: { uint8_t val = readMemory(cpu.getHL()); uint16_t res = (uint16_t)cpu.A - val; updateFlagsSub(cpu.A, val, res); cpu.A = (uint8_t)res; cycles = 7; break; }
        case 0x97: { uint16_t res = (uint16_t)cpu.A - cpu.A; updateFlagsSub(cpu.A, cpu.A, res); cpu.A = (uint8_t)res; break; }

        case 0x98: { // SBC A, B
            uint16_t res = (uint16_t)cpu.A - cpu.B - ((cpu.F & Z80A::C_BIT) ? 1 : 0);
            updateFlagsSbc(cpu.A, cpu.B, res);
            cpu.A = (uint8_t)res;
            break;
        }
        case 0x99: { uint16_t res = (uint16_t)cpu.A - cpu.C - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, cpu.C, res); cpu.A = (uint8_t)res; break; }
        case 0x9A: { uint16_t res = (uint16_t)cpu.A - cpu.D - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, cpu.D, res); cpu.A = (uint8_t)res; break; }
        case 0x9B: { uint16_t res = (uint16_t)cpu.A - cpu.E - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, cpu.E, res); cpu.A = (uint8_t)res; break; }
        case 0x9C: { uint16_t res = (uint16_t)cpu.A - cpu.H - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, cpu.H, res); cpu.A = (uint8_t)res; break; }
        case 0x9D: { uint16_t res = (uint16_t)cpu.A - cpu.L - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, cpu.L, res); cpu.A = (uint8_t)res; break; }
        case 0x9E: { uint8_t val = readMemory(cpu.getHL()); uint16_t res = (uint16_t)cpu.A - val - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, val, res); cpu.A = (uint8_t)res; cycles = 7; break; }
        case 0x9F: { uint16_t res = (uint16_t)cpu.A - cpu.A - ((cpu.F & Z80A::C_BIT) ? 1 : 0); updateFlagsSbc(cpu.A, cpu.A, res); cpu.A = (uint8_t)res; break; }
        case 0xDE: { // SBC A, n
            uint8_t n = readMemory(cpu.PC++);
            uint16_t res = (uint16_t)cpu.A - n - ((cpu.F & Z80A::C_BIT) ? 1 : 0);
            updateFlagsSbc(cpu.A, n, res);
            cpu.A = (uint8_t)res;
            cycles = 7;
            break;
        }

        case 0xD6: { // SUB n
            uint8_t n = readMemory(cpu.PC++);
            uint16_t result = cpu.A - n;
            updateFlagsSub(cpu.A, n, result);
            cpu.A = (uint8_t)result;
            cycles = 7;
            break;
        }

        // --- Logic Group ---
        case 0xA0: { // AND B 
           cpu.A &= cpu.B;
           updateSZP(cpu.A);
           cpu.F &= ~Z80A::C_BIT;
           cpu.F |= Z80A::H_BIT;
           cpu.F &= ~Z80A::N_BIT;
           break;
        }
        case 0xA1: {  // AND C 
          cpu.A &= cpu.C;
          updateSZP(cpu.A);
          cpu.F &= ~Z80A::C_BIT;
          cpu.F |= Z80A::H_BIT;
          cpu.F &= ~Z80A::N_BIT;
          break;
        }
        case 0xA2: { cpu.A &= cpu.D; updateSZP(cpu.A); cpu.F &= ~Z80A::C_BIT; cpu.F |= Z80A::H_BIT; cpu.F &= ~Z80A::N_BIT; break; }
        case 0xA3: { cpu.A &= cpu.E; updateSZP(cpu.A); cpu.F &= ~Z80A::C_BIT; cpu.F |= Z80A::H_BIT; cpu.F &= ~Z80A::N_BIT; break; }
        case 0xA4: { cpu.A &= cpu.H; updateSZP(cpu.A); cpu.F &= ~Z80A::C_BIT; cpu.F |= Z80A::H_BIT; cpu.F &= ~Z80A::N_BIT; break; }
        case 0xA5: { cpu.A &= cpu.L; updateSZP(cpu.A); cpu.F &= ~Z80A::C_BIT; cpu.F |= Z80A::H_BIT; cpu.F &= ~Z80A::N_BIT; break; }
        case 0xA6: {  //AND (HL)
          cpu.A &= readMemory(cpu.getHL());
          updateSZP(cpu.A);
          cpu.F &= ~Z80A::C_BIT;
          cpu.F |= Z80A::H_BIT;
          cpu.F &= ~Z80A::N_BIT;
          cycles = 7;
          break;
        }
        case 0xA7: { // AND A
          cpu.A &= cpu.A;
          updateSZP(cpu.A);
          cpu.F &= ~Z80A::C_BIT;
          cpu.F |= Z80A::H_BIT;
          cpu.F &= ~Z80A::N_BIT;
          break; 
        }
        case 0xAE: { // XOR (HL)
          cpu.A ^= readMemory(cpu.getHL());
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          cycles = 7;
          break;
        }
        case 0xB0: {  // OR B
          cpu.A |= cpu.B;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xB1: {  // OR C 
             cpu.A |= cpu.C;
             updateSZP(cpu.A);
             cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
             break;
        }
        case 0xB2: { // OR D
          cpu.A |= cpu.D;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xB3: { // OR E
          cpu.A |= cpu.E;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xB4: { // OR H
          cpu.A |= cpu.H;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xB5: { // OR L
          cpu.A |= cpu.L;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xB6: { // OR (HL)
          cpu.A |= readMemory(cpu.getHL());
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          cycles = 7;
          break;
        }
        case 0xB7: { // OR A
          cpu.A |= cpu.A;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xB8: { // CP B
          updateFlagsSub(cpu.A, cpu.B, (uint16_t)cpu.A - cpu.B);
          break;
        }
        case 0xB9: { // CP C
          updateFlagsSub(cpu.A, cpu.C, (uint16_t)cpu.A - cpu.C);
          break;
        }
        case 0xBA: { // CP D
          updateFlagsSub(cpu.A, cpu.D, (uint16_t)cpu.A - cpu.D);
          break;
        }
        case 0xBB: { // CP E
          updateFlagsSub(cpu.A, cpu.E, (uint16_t)cpu.A - cpu.E);
          break;
        }
        case 0xBC: { // CP H
          updateFlagsSub(cpu.A, cpu.H, (uint16_t)cpu.A - cpu.H);
          break;
        }
        case 0xBD: { // CP L
          updateFlagsSub(cpu.A, cpu.L, (uint16_t)cpu.A - cpu.L);
          break;
        }
        case 0xBE: { // CP (HL)
          uint8_t val = readMemory(cpu.getHL());
          updateFlagsSub(cpu.A, val, (uint16_t)cpu.A - val);
          cycles = 7;
          break;
        }
        case 0xBF: { // CP A
          updateFlagsSub(cpu.A, cpu.A, 0);
          break;
        }
        case 0xE6: {  // AND n
          uint8_t n = readMemory(cpu.PC++);
          cpu.A &= n;
          updateSZP(cpu.A);
          cpu.F &= ~Z80A::C_BIT;
          cpu.F |= Z80A::H_BIT;
          cpu.F &= ~Z80A::N_BIT;
          cycles = 7;
          break;
        } 
 
        case 0xC9: { // RET
            uint16_t ret_addr = read16(cpu.SP);
            cpu.PC = ret_addr;
            cpu.SP += 2;
            std::cout << "RET: returning to 0x" << std::hex << cpu.PC 
                      << " (popped from SP=0x" << (cpu.SP-2) << ")" << std::dec << std::endl;
            cycles = 10;
            break;
        }

        case 0xC0: { // RET NZ
            if (!(cpu.F & Z80A::Z_BIT)) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xC8: { // RET Z
            if (cpu.F & Z80A::Z_BIT) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xD0: { // RET NC
            if (!(cpu.F & Z80A::C_BIT)) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xD8: { // RET C
            if (cpu.F & Z80A::C_BIT) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xE0: { // RET PO
            if (!(cpu.F & Z80A::P_BIT)) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xE8: { // RET PE
            if (cpu.F & Z80A::P_BIT) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xF0: { // RET P
            if (!(cpu.F & Z80A::S_BIT)) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xF8: { // RET M
            if (cpu.F & Z80A::S_BIT) {
                cpu.PC = read16(cpu.SP);
                cpu.SP += 2;
                cycles = 11;
            } else {
                cycles = 5;
            }
            break;
        }

        case 0xD3: { // OUT (n), A
            uint8_t port = readMemory(cpu.PC++);
            writeIO(port, cpu.A);
            cycles = 11;
            break;
        }

        case 0xDB: { // IN A, (n)
            uint8_t port = readMemory(cpu.PC++);
            cpu.A = readIO(port);
            cycles = 11;
            break;
        }

        case 0xF3: { // DI
            cpu.IFF1 = cpu.IFF2 = false;
            cycles = 4;
            break;
        }

        case 0xFB: { // EI
            // Set pending flag to enable interrupts after the next instruction
            cpu.EI_pending = true;
            cycles = 4;
            break;
        }

        case 0xAF: {  // XOR A (Clears A)
          cpu.A = 0;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          break;
        }
        case 0xA8: { cpu.A ^= cpu.B; updateSZP(cpu.A); cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT); break; }
        case 0xA9: { cpu.A ^= cpu.C; updateSZP(cpu.A); cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT); break; }
        case 0xAA: { cpu.A ^= cpu.D; updateSZP(cpu.A); cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT); break; }
        case 0xAB: { cpu.A ^= cpu.E; updateSZP(cpu.A); cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT); break; }
        case 0xAC: { cpu.A ^= cpu.H; updateSZP(cpu.A); cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT); break; }
        case 0xEE: { // XOR n (Immediate)
             uint8_t n = readMemory(cpu.PC++);
             cpu.A ^= n;
             updateSZP(cpu.A);
             cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
             cycles = 7;
             break;
       }

        case 0xF6: {  // OR n
          uint8_t n = readMemory(cpu.PC++);
          cpu.A |= n;
          updateSZP(cpu.A);
          cpu.F &= ~(Z80A::C_BIT | Z80A::H_BIT | Z80A::N_BIT);
          cycles = 7;
          break;
        }

        // --- Compare Group ---
        // Os casos 0xB8-0xBF já foram tratados acima
        case 0xFE: { // CP n 
            uint8_t n = readMemory(cpu.PC++);
            updateFlagsSub(cpu.A, n, (uint16_t)cpu.A - n);
            cycles = 7;
            break;
        }

        // --- Increment/Decrement Group ---
        case 0x3C: { // INC A 
            uint8_t before = cpu.A;
            cpu.A++;
            cpu.F &= Z80A::C_BIT; // Preserve carry 
            if (cpu.A == 0) cpu.F |= Z80A::Z_BIT;
            if (cpu.A & 0x80) cpu.F |= Z80A::S_BIT;
            if ((before & 0x0F) == 0x0F) cpu.F |= Z80A::H_BIT;
            if (before == 0x7F) cpu.F |= Z80A::P_BIT;  // Overflow 
            break;
        }           
        case 0x04: { // INC B
          uint8_t old = cpu.B;
          cpu.B++;
          uint8_t f = cpu.F & Z80A::C_BIT; // Preserve Carry
          if (cpu.B == 0) f |= Z80A::Z_BIT;
          if (cpu.B & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT;
          if (old == 0x7F) f |= Z80A::P_BIT; // Overflow
          cpu.F = f;
          break;
        }
        case 0x0C: { // INC C
          uint8_t old = cpu.C;
          cpu.C++;
          uint8_t f = cpu.F & Z80A::C_BIT;
          if (cpu.C == 0) f |= Z80A::Z_BIT;
          if (cpu.C & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT;
          if (old == 0x7F) f |= Z80A::P_BIT;
          cpu.F = f;
          break;
        }
        case 0x14: { // INC D
          uint8_t old = cpu.D; cpu.D++;
          uint8_t f = cpu.F & Z80A::C_BIT;
          if (cpu.D == 0) f |= Z80A::Z_BIT; if (cpu.D & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT; if (old == 0x7F) f |= Z80A::P_BIT;
          cpu.F = f; break;
        }
        case 0x1C: { // INC E
          uint8_t old = cpu.E; cpu.E++;
          uint8_t f = cpu.F & Z80A::C_BIT;
          if (cpu.E == 0) f |= Z80A::Z_BIT; if (cpu.E & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT; if (old == 0x7F) f |= Z80A::P_BIT;
          cpu.F = f; break;
        }
        case 0x24: { // INC H
          uint8_t old = cpu.H;
          cpu.H++;
          uint8_t f = cpu.F & Z80A::C_BIT;
          if (cpu.H == 0) f |= Z80A::Z_BIT;
          if (cpu.H & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT;
          if (old == 0x7F) f |= Z80A::P_BIT;
          cpu.F = f;
          break;
        }
        case 0x2C: { // INC L
          uint8_t old = cpu.L;
          cpu.L++;
          uint8_t f = cpu.F & Z80A::C_BIT;
          if (cpu.L == 0) f |= Z80A::Z_BIT;
          if (cpu.L & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT;
          if (old == 0x7F) f |= Z80A::P_BIT;
          cpu.F = f;
          break;
        }
        case 0x34: { // INC (HL)
          uint8_t old = readMemory(cpu.getHL());
          uint8_t val = old + 1;
          writeMemory(cpu.getHL(), val);
          uint8_t f = cpu.F & Z80A::C_BIT;
          if (val == 0) f |= Z80A::Z_BIT; if (val & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x0F) f |= Z80A::H_BIT; if (old == 0x7F) f |= Z80A::P_BIT;
          cpu.F = f; cycles = 11; break;
        }

        case 0x05: { // DEC B
          uint8_t old = cpu.B;
          cpu.B--;
          uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
          if (cpu.B == 0) f |= Z80A::Z_BIT;
          if (cpu.B & 0x80) f |= Z80A::S_BIT;
          if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT;
          if (old == 0x80) f |= Z80A::P_BIT; // Overflow
          cpu.F = f;
          break;
        }
        case 0x0D: { // DEC C
            uint8_t old = cpu.C;
            cpu.C--;
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (cpu.C == 0) f |= Z80A::Z_BIT;
            if (cpu.C & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT;
            if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f;
            break;
        }
        case 0x15: { // DEC D
            uint8_t old = cpu.D; cpu.D--;
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (cpu.D == 0) f |= Z80A::Z_BIT; if (cpu.D & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT; if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f; break;
        }
        case 0x1D: { // DEC E
            uint8_t old = cpu.E; cpu.E--;
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (cpu.E == 0) f |= Z80A::Z_BIT; if (cpu.E & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT; if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f; break;
        }
        case 0x25: { // DEC H
            uint8_t old = cpu.H; cpu.H--;
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (cpu.H == 0) f |= Z80A::Z_BIT; if (cpu.H & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT; if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f; break;
        }
        case 0x2D: { // DEC L
            uint8_t old = cpu.L; cpu.L--;
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (cpu.L == 0) f |= Z80A::Z_BIT; if (cpu.L & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT; if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f; break;
        }
        case 0x35: { // DEC (HL)
            uint8_t old = readMemory(cpu.getHL());
            uint8_t val = old - 1;
            writeMemory(cpu.getHL(), val);
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (val == 0) f |= Z80A::Z_BIT; if (val & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT; if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f; cycles = 11; break;
        }
        case 0x3D: { // DEC A
            uint8_t old = cpu.A; cpu.A--;
            uint8_t f = (cpu.F & Z80A::C_BIT) | Z80A::N_BIT;
            if (cpu.A == 0) f |= Z80A::Z_BIT; if (cpu.A & 0x80) f |= Z80A::S_BIT;
            if ((old & 0x0F) == 0x00) f |= Z80A::H_BIT; if (old == 0x80) f |= Z80A::P_BIT;
            cpu.F = f; break;
        }

        // --- 16-bit Arithmetic ---
        case 0x09: { // ADD HL, BC
          uint32_t hl = cpu.getHL();
          uint32_t bc = cpu.getBC();
          uint32_t res = hl + bc;
          cpu.F &= ~(Z80A::N_BIT | Z80A::H_BIT | Z80A::C_BIT);
          if (((hl & 0x0FFF) + (bc & 0x0FFF)) & 0x1000) cpu.F |= Z80A::H_BIT;
          if (res & 0x10000) cpu.F |= Z80A::C_BIT;
          cpu.setHL(res & 0xFFFF);
          cycles = 11;
          break;
        }
        case 0x19: { // ADD HL, DE
          uint32_t hl = cpu.getHL();
          uint32_t de = cpu.getDE();
          uint32_t res = hl + de;
          cpu.F &= ~(Z80A::N_BIT | Z80A::H_BIT | Z80A::C_BIT);
          if (((hl & 0x0FFF) + (de & 0x0FFF)) & 0x1000) cpu.F |= Z80A::H_BIT;
          if (res & 0x10000) cpu.F |= Z80A::C_BIT;
          cpu.setHL(res & 0xFFFF);
          cycles = 11;
          break;
        }
        case 0x29: { // ADD HL, HL
         uint32_t hl = cpu.getHL();
         uint32_t res = hl + hl;
         cpu.F &= ~(Z80A::N_BIT | Z80A::H_BIT | Z80A::C_BIT);
         if (((hl & 0x0FFF) + (hl & 0x0FFF)) & 0x1000) cpu.F |= Z80A::H_BIT;
         if (res & 0x10000) cpu.F |= Z80A::C_BIT;
         cpu.setHL(res & 0xFFFF);
         cycles = 11;
         break;
        }
        case 0x39: { // ADD HL, SP
         uint32_t hl = cpu.getHL();
         uint32_t sp = cpu.SP;
         uint32_t res = hl + sp;
         cpu.F &= ~(Z80A::N_BIT | Z80A::H_BIT | Z80A::C_BIT);
         if (((hl & 0x0FFF) + (sp & 0x0FFF)) & 0x1000) cpu.F |= Z80A::H_BIT;
         if (res & 0x10000) cpu.F |= Z80A::C_BIT;
         cpu.setHL(res & 0xFFFF);
         cycles = 11;
         break;
       }
       case 0x03: { cpu.setBC(cpu.getBC() + 1); cycles = 6; break; } // INC BC
       case 0x13: { cpu.setDE(cpu.getDE() + 1); cycles = 6; break; } // INC DE
       case 0x23: { cpu.setHL(cpu.getHL() + 1); cycles = 6; break; } // INC HL
       case 0x33: { cpu.SP++; cycles = 6; break; }                   // INC SP
       case 0x0B: { cpu.setBC(cpu.getBC() - 1); cycles = 6; break; } // DEC BC
       case 0x1B: { cpu.setDE(cpu.getDE() - 1); cycles = 6; break; } // DEC DE
       case 0x2B: { cpu.setHL(cpu.getHL() - 1); cycles = 6; break; } // DEC HL
       case 0x3B: { cpu.SP--; cycles = 6; break; }                   // DEC SP

        default:
            debug_log << "Unimplemented Opcode: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            // Para opcodes não implementados, apenas incrementar PC e usar ciclos padrão
            // Alguns opcodes podem ser de 1 byte, mas não sabemos
            // Para segurança, não fazer nada além de log
            break;
    }
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

int OpcodesHandler::getCycles() const {
    return cycles;
}
