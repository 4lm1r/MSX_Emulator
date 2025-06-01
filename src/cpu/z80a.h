#ifndef Z80A_H
#define Z80A_H

#include <cstdint>
#include <functional>
#include "debug.h"
#include "opcodesHandler.h" // Added for OpcodesHandler

class OpcodesHandler;

class Z80A {
public:
    Z80A();
    void reset();
    int execute();
    void setMemoryReadCallback(std::function<uint8_t(uint16_t)> callback);
    void setMemoryWriteCallback(std::function<void(uint16_t, uint8_t)> callback);
    void setIOReadCallback(std::function<uint8_t(uint8_t)> callback);
    void setIOWriteCallback(std::function<void(uint8_t, uint8_t)> callback);
    void triggerInterrupt();

    bool halted = false;
    bool isHalted() const { return halted; }

    // Main Registers
    uint8_t A = 0, B = 0, C = 0, D = 0, E = 0, H = 0, L = 0;
    uint8_t F = 0; // Flags: S, Z, H, P/V, N, C (bit 7 to 0)
    uint16_t PC = 0;
    uint16_t SP = 0xFFFE;

    // Alternate Registers
    uint8_t A_ = 0, F_ = 0, B_ = 0, C_ = 0, D_ = 0, E_ = 0, H_ = 0, L_ = 0;

    // Index Registers
    uint16_t IX = 0, IY = 0;

    // Interrupt and Refresh Registers
    uint8_t I = 0, R = 0;

    // Helper to access HL, BC, DE as 16-bit pairs
    uint16_t getHL() const { return (H << 8) | L; }
    void setHL(uint16_t value) { H = (value >> 8) & 0xFF; L = value & 0xFF; }
    uint16_t getBC() const { return (B << 8) | C; }
    void setBC(uint16_t value) { B = (value >> 8) & 0xFF; C = value & 0xFF; }
    uint16_t getDE() const { return (D << 8) | E; }
    void setDE(uint16_t value) { D = (value >> 8) & 0xFF; E = value & 0xFF; }

    bool IFF1 = false; // Main interrupt enable
    bool IFF2 = false; // Backup for NMI/RETN
    bool interruptPending = false; // For delayed EI
    uint64_t total_cycles = 0;
    uint16_t last_call_return = 0x0000;

    friend class OpcodesHandler;

private:
    std::function<uint8_t(uint16_t)> memoryReadCallback;
    std::function<void(uint16_t, uint8_t)> memoryWriteCallback;
    std::function<uint8_t(uint8_t)> ioReadCallback;
    std::function<void(uint8_t, uint8_t)> ioWriteCallback;
    OpcodesHandler* opcodeHandler = nullptr;
};

#endif
