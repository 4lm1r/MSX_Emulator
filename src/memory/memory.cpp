#include "memory.h"
#include <cstring>

Memory::Memory() {
    reset();
}

void Memory::reset() {
    std::memset(rom, 0, ROM_SIZE);
    std::memset(ram, 0, RAM_SIZE);
    // Initialize ROM with a simple boot sequence (e.g., JP 0x0000)
    rom[0] = 0xC3; // JP
    rom[1] = 0x00;
    rom[2] = 0x00;
}

uint8_t Memory::read(uint16_t addr) const {
    if (addr < ROM_SIZE) {
        return rom[addr];
    }
    return ram[addr - ROM_SIZE];
}

void Memory::write(uint16_t addr, uint8_t value) {
    if (addr >= ROM_SIZE) {
        ram[addr - ROM_SIZE] = value;
    }
    // ROM is read-only, ignore writes
}

void Memory::write(uint16_t addr, uint8_t value) const {
    if (addr < ROM_SIZE) {
        rom[addr] = value; // Allow ROM writes for initial setup
    } else {
        ram[addr - ROM_SIZE] = value; // Allow RAM writes for initial setup
    }
}

Memory& Memory::getInstance() {
    static Memory instance;
    return instance;
}
