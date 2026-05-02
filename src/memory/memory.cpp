#include "memory.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <unistd.h>

// --- RomBlock ---
RomBlock::RomBlock(size_t size) { buffer.resize(size, 0xFF); }
uint8_t RomBlock::read(uint16_t addr) const {
    if (addr < buffer.size()) return buffer[addr];
    return 0xFF;
}
void RomBlock::write(uint16_t addr, uint8_t value) { /* ROM is read-only */ }
bool RomBlock::load(const std::string& filename, uint16_t offset) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    size_t max_read = buffer.size() - offset;
    if (file_size > max_read) file_size = max_read;
    file.read(reinterpret_cast<char*>(&buffer[offset]), file_size);
    std::cout << "RomBlock: Loaded " << file_size << " bytes from " << filename << " at offset 0x" << std::hex << offset << std::dec << std::endl;
    // Fill the rest of the 64KB with 0x00 to avoid false expansion detection at 0xFFFF
    for (size_t i = offset + file_size; i < buffer.size(); ++i) buffer[i] = 0x00;
    return true;
}

// --- RamBlock ---
RamBlock::RamBlock(size_t size) { buffer.resize(size, 0x00); }
uint8_t RamBlock::read(uint16_t addr) const {
    if (addr < buffer.size()) return buffer[addr];
    return 0xFF;
}
void RamBlock::write(uint16_t addr, uint8_t value) {
    if (addr < buffer.size()) buffer[addr] = value;
}

// --- Memory ---
Memory& Memory::getInstance() {
    static Memory instance;
    return instance;
}

Memory::Memory() : current_ppi_val(0xF0) {
    reset();
}

void Memory::reset() {
    std::cout << "Memory::reset: Standard MSX1 layout (Slot 0=ROM, Slot 3=Expanded RAM)" << std::endl;
    for (int i = 0; i < 4; i++) {
        slots[i].expanded = false;
        slots[i].secondary_slot_register = 0;
        for (int j = 0; j < 4; j++) slots[i].subslots[j] = nullptr;
    }
    slots[0].expanded = false;
    slots[0].subslots[0] = std::make_shared<RomBlock>(64 * 1024);
    slots[3].expanded = true;
    slots[3].secondary_slot_register = 0x00;
    auto main_ram = std::make_shared<RamBlock>(64 * 1024);
    for (int j = 0; j < 4; j++) slots[3].subslots[j] = main_ram;
    mapPrimarySlots(0xF0);
}

uint8_t Memory::getActiveSecondarySlot(int primary_slot, int page) const {
    if (!slots[primary_slot].expanded) return 0;
    return (slots[primary_slot].secondary_slot_register >> (page * 2)) & 0x03;
}

uint8_t Memory::read(uint16_t addr) const {
    int page = (addr >> 14) & 0x03;
    int primary_slot = (current_ppi_val >> (page * 2)) & 0x03;

    if (addr == 0xFFFF) {
        if (slots[primary_slot].expanded) {
            return ~slots[primary_slot].secondary_slot_register;
        } else if (primary_slot == 0) {
            return 0x00; // Force 0x00 for Slot 0 to avoid false detection
        }
    }

    uint8_t subslot = getActiveSecondarySlot(primary_slot, page);
    if (slots[primary_slot].subslots[subslot]) {
        return slots[primary_slot].subslots[subslot]->read(addr);
    }
    return 0xFF;
}

void Memory::write(uint16_t addr, uint8_t value) {
    int page = (addr >> 14) & 0x03;
    int primary_slot = (current_ppi_val >> (page * 2)) & 0x03;

    if (addr == 0xFFFF && slots[primary_slot].expanded) {
        slots[primary_slot].secondary_slot_register = value;
        return;
    }

    uint8_t subslot = getActiveSecondarySlot(primary_slot, page);
    if (slots[primary_slot].subslots[subslot]) {
        slots[primary_slot].subslots[subslot]->write(addr, value);
    }
}

void Memory::mapPrimarySlots(uint8_t ppi_val) {
    current_ppi_val = ppi_val;
}

bool Memory::loadROM(const std::string& filename, uint16_t addr) {
    auto rom = std::dynamic_pointer_cast<RomBlock>(slots[0].subslots[0]);
    if (rom) return rom->load(filename, addr);
    return false;
}

bool Memory::loadCartridge(const std::string& filename) {
    // Standard MSX cartridge: ROM mapped into slot 1, pages 1–2 (0x4000–0xBFFF).
    // Load the file at offset 0x4000 within a 64 KB ROM block so the CPU
    // sees it at the correct address when slot 1 is active.
    auto rom = std::make_shared<RomBlock>(64 * 1024);
    if (!rom->load(filename, 0x4000)) return false;
    slots[1].expanded = false;
    slots[1].subslots[0] = rom;
    std::cout << "Memory: Cartridge loaded into slot 1 from " << filename << std::endl;
    return true;
}
