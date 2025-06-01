#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

class Memory {
public:
    Memory();
    void reset(); // Clear RAM, initialize ROM
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t value);
    void write(uint16_t addr, uint8_t value) const; // For initial ROM and RAM setup

    // Get global Memory instance
    static Memory& getInstance();

private:
    static constexpr uint32_t MEMORY_SIZE = 0x10000; // 64 KB
    static constexpr uint32_t ROM_SIZE = 0x8000; // 32 KB
    static constexpr uint32_t RAM_SIZE = 0x8000; // 32 KB
    mutable uint8_t rom[ROM_SIZE]; // 0000-7FFF, mutable for ROM setup
    mutable uint8_t ram[RAM_SIZE]; // 8000-FFFF, mutable for RAM setup
};

#endif // MEMORY_H
