#include "z80a.h"
  #include "opcodesHandler.h"
  #include "debug.h"

  Z80A::Z80A() : A(0), B(0), C(0), D(0), E(0), H(0), L(0), F(0), PC(0), SP(0xF380),
                 A_(0), F_(0), B_(0), C_(0), D_(0), E_(0), H_(0), L_(0),
                 IX(0), IY(0), I(0), R(0),
                 halted(false), opcodeHandler(nullptr), IFF1(false), IFF2(false),
                 interruptPending(false), total_cycles(0), last_call_return(0x0000) {}

  void Z80A::reset() {
      A = B = C = D = E = H = L = F = 0;
      A_ = F_ = B_ = C_ = D_ = E_ = H_ = L_ = 0;
      IX = IY = I = R = 0;
      PC = 0x0000;
      SP = 0xF380; // MSX standard initial stack pointer
      halted = false;
      IFF1 = IFF2 = false;
      interruptPending = false;
      total_cycles = 0;
      debug_log << "CPU reset, initial SP=0x" << std::hex << SP << std::dec << std::endl;
  }

  int Z80A::execute() {
      if (halted) {
          return 4;
      }

      if (interruptPending) {
          IFF1 = IFF2 = true;
          interruptPending = false;
      }

      uint8_t opcode = memoryReadCallback(PC);
      int cycles = 4; // Default cycle count

      switch (opcode) {
          case 0xF3: // DI
              IFF1 = IFF2 = false;
              PC++;
              return 4;
          case 0xFB: // EI
              interruptPending = true;
              PC++;
              return 4;
      }

      OpcodesHandler handler(*this);
      handler.executeOpcode(opcode);
      cycles = handler.getCycles(); // Use OpcodesHandler's cycle count

      total_cycles += cycles;
      return cycles;
  }

  void Z80A::setMemoryReadCallback(std::function<uint8_t(uint16_t)> callback) {
      memoryReadCallback = callback;
  }

  void Z80A::setMemoryWriteCallback(std::function<void(uint16_t, uint8_t)> callback) {
      memoryWriteCallback = callback;
  }

  void Z80A::setIOReadCallback(std::function<uint8_t(uint8_t)> callback) {
    ioReadCallback = [callback](uint8_t port) -> uint8_t {
        if (port == 0x99 || port == 0x98) {
            debug_log << "Reading VDP port 0x" << std::hex << (int)port << std::dec << std::endl;
            return callback(port);
        }
        debug_log << "Reading unmapped port 0x" << std::hex << (int)port << ": returning 0xFF" << std::dec << std::endl;
        return 0xFF;
    };
}

  void Z80A::setIOWriteCallback(std::function<void(uint8_t, uint8_t)> callback) {
    ioWriteCallback = [callback](uint8_t port, uint8_t value) {
        // Map BIOS ports to VDP ports
        if (port == 0xAB) {
            debug_log << "Writing VDP control port (0x99): 0x" << std::hex << (int)value << std::dec << std::endl;
            callback(0x99, value); // Map 0xAB to 0x99 (VDP control)
        } else if (port == 0xAA) {
            debug_log << "Writing VDP data port (0x98): 0x" << std::hex << (int)value << std::dec << std::endl;
            callback(0x98, value); // Map 0xAA to 0x98 (VDP data)
        } else if (port == 0xFF || port == 0xFE || port == 0xFD || port == 0xFC) {
            debug_log << "Ignoring unmapped port 0x" << std::hex << (int)port << ": 0x" << (int)value << std::dec << std::endl;
        } else {
            debug_log << "Writing unmapped port 0x" << std::hex << (int)port << ": 0x" << (int)value << std::dec << std::endl;
            callback(port, value);
        }
    };
}
  void Z80A::triggerInterrupt() {
    if (!IFF1 || halted) return;
    IFF1 = IFF2 = false;
    debug_log << "Interrupt triggered, pushing PC=0x" << std::hex << PC << std::dec << std::endl;
    SP -= 2;
    memoryWriteCallback(SP + 1, (PC >> 8) & 0xFF);
    memoryWriteCallback(SP, PC & 0xFF);
    PC = 0x0038;
    if (ioReadCallback) {
        uint8_t status = ioReadCallback(0x99); // Read VDP status to clear interrupt
        debug_log << "Read VDP status to clear interrupt: 0x" << std::hex << (int)status << std::dec << std::endl;
    }
    total_cycles += 11;
    debug_log << "Interrupt triggered, jumping to 0x0038" << std::endl;
}
