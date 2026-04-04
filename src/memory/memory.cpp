#include "memory.h"
#include <fstream>
#include <iostream>

// --- RomSlot ---
RomSlot::RomSlot(size_t size) { buffer.resize(size, 0); }
uint8_t RomSlot::read(uint16_t addr) const {
    if (addr < buffer.size()) return buffer[addr];
    return 0xFF;
}
void RomSlot::write(uint16_t addr, uint8_t value) { /* ROM is read-only */ }
bool RomSlot::load(const std::string& filename, uint16_t offset) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    file.read(reinterpret_cast<char*>(&buffer[offset]), buffer.size() - offset);
    return true;
}
void RomSlot::setData(const std::vector<uint8_t>& data, uint16_t offset) {
    for (size_t i = 0; i < data.size() && (offset + i) < buffer.size(); ++i) {
        buffer[offset + i] = data[i];
    }
}

// --- RamSlot ---
RamSlot::RamSlot(size_t size) { buffer.resize(size, 0); }
uint8_t RamSlot::read(uint16_t addr) const {
    if (addr < buffer.size()) return buffer[addr];
    return 0xFF;
}
void RamSlot::write(uint16_t addr, uint8_t value) {
    if (addr < buffer.size()) buffer[addr] = value;
}

// --- Memory ---
Memory& Memory::getInstance() {
    static Memory instance;
    return instance;
}

Memory::Memory() : current_ppi_val(0) {
    reset();
}

void Memory::reset() {
    // Default setup: Slot 0 = ROM, Slot 1 = RAM, Slots 2,3 = Empty
    primary_slots[0] = std::make_shared<RomSlot>(64 * 1024);
    primary_slots[1] = std::make_shared<RamSlot>(64 * 1024);
    primary_slots[2] = std::make_shared<RamSlot>(64 * 1024); // More RAM for now
    primary_slots[3] = std::make_shared<RamSlot>(64 * 1024);
    
    mapPrimarySlots(0xC0); // Page 0,1,2 = Slot 0, Page 3 = Slot 3 (RAM)
}

uint8_t Memory::read(uint16_t addr) const {
    int page = (addr >> 14) & 0x03;
    if (page_map[page]) return page_map[page]->read(addr);
    return 0xFF;
}

void Memory::write(uint16_t addr, uint8_t value) {
    int page = (addr >> 14) & 0x03;
    if (page_map[page]) page_map[page]->write(addr, value);
}

void Memory::setSlot(int slot_num, std::shared_ptr<Slot> slot) {
    if (slot_num >= 0 && slot_num < 4) {
        primary_slots[slot_num] = slot;
        mapPrimarySlots(current_ppi_val); // Refresh mapping
    }
}

void Memory::mapPrimarySlots(uint8_t ppi_val) {
    current_ppi_val = ppi_val;
    // ppi_val = [D7 D6] [D5 D4] [D3 D2] [D1 D0]
    //            Page 3  Page 2  Page 1  Page 0
    for (int page = 0; page < 4; ++page) {
        int slot_index = (ppi_val >> (page * 2)) & 0x03;
        page_map[page] = primary_slots[slot_index];
    }
}

// Helper: load into Slot 0 for BIOS
bool Memory::loadROM(const std::string& filename, uint16_t addr) {
    auto rom = std::dynamic_pointer_cast<RomSlot>(primary_slots[0]);
    if (rom) return rom->load(filename, addr);
    return false;
}

void Memory::setupMemory(const std::vector<uint8_t>& data, uint16_t addr) {
    // For tests/simple setup, we use Slot 0 (ROM) but it might be read-only.
    // Let's use Slot 1 (RAM) for tests to ensure writes work.
    auto ram = std::dynamic_pointer_cast<RamSlot>(primary_slots[1]);
    if (ram) {
        for (size_t i = 0; i < data.size(); ++i) ram->write(addr + i, data[i]);
    }
}
