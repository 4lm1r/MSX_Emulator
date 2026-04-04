#include "memory.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <unistd.h>

// --- RomSlot ---
RomSlot::RomSlot(size_t size) { buffer.resize(size, 0); }
uint8_t RomSlot::read(uint16_t addr) const {
    if (addr < buffer.size()) return buffer[addr];
    return 0xFF;
}
void RomSlot::write(uint16_t addr, uint8_t value) { /* ROM is read-only */ }
bool RomSlot::load(const std::string& filename, uint16_t offset) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "RomSlot::load: Failed to open file: " << filename << std::endl;
        // Try to get current working directory
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            std::cout << "Current working directory: " << cwd << std::endl;
        }
        return false;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cout << "RomSlot::load: File '" << filename << "' size: " << file_size << " bytes" << std::endl;
    
    // Ensure we don't read beyond buffer
    size_t max_read = buffer.size() - offset;
    if (file_size > max_read) {
        std::cout << "RomSlot::load: Warning: file too large, truncating to " << max_read << " bytes" << std::endl;
        file_size = max_read;
    }
    
    file.read(reinterpret_cast<char*>(&buffer[offset]), file_size);
    
    // Log first 16 bytes
    std::cout << "RomSlot::load: First 16 bytes at offset 0x" << std::hex << offset << ": ";
    for (int i = 0; i < 16 && i < file_size; i++) {
        std::cout << std::setw(2) << std::setfill('0') << (int)buffer[offset + i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Also log bytes at 0x4000 (BASIC ROM area)
    if (file_size > 0x4000) {
        std::cout << "RomSlot::load: First 16 bytes of BASIC ROM at 0x4000: ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::setw(2) << std::setfill('0') << (int)buffer[0x4000 + i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
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
    std::cout << "Memory::reset: Initializing slots..." << std::endl;
    // Default setup: Slot 0 = ROM, Slot 1 = RAM, Slots 2,3 = Empty
    primary_slots[0] = std::make_shared<RomSlot>(64 * 1024);
    primary_slots[1] = std::make_shared<RamSlot>(64 * 1024);
    primary_slots[2] = std::make_shared<RamSlot>(64 * 1024); // More RAM for now
    primary_slots[3] = std::make_shared<RamSlot>(64 * 1024);
    
    std::cout << "  Slot 0: ROM (64KB)" << std::endl;
    std::cout << "  Slot 1: RAM (64KB)" << std::endl;
    std::cout << "  Slot 2: RAM (64KB)" << std::endl;
    std::cout << "  Slot 3: RAM (64KB)" << std::endl;
    
    mapPrimarySlots(0xC0); // Page 0,1,2 = Slot 0, Page 3 = Slot 3 (RAM)
}

uint8_t Memory::read(uint16_t addr) const {
    int page = (addr >> 14) & 0x03;
    if (page_map[page]) return page_map[page]->read(addr);
    return 0xFF;
}

void Memory::write(uint16_t addr, uint8_t value) {
    int page = (addr >> 14) & 0x03;
    // Log first few writes to RAM areas
    static int write_count = 0;
    if (write_count < 10) {
        std::cout << "MEM write: addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr 
                  << " page=" << page << " slot=" << ((current_ppi_val >> (page*2)) & 0x03)
                  << " val=0x" << std::setw(2) << (int)value << std::dec << std::endl;
        write_count++;
    }
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
    static int map_count = 0;
    if (map_count < 5) {
        std::cout << "Memory::mapPrimarySlots: ppi_val=0x" << std::hex << (int)ppi_val << std::dec << std::endl;
        for (int page = 0; page < 4; ++page) {
            int slot_index = (ppi_val >> (page * 2)) & 0x03;
            page_map[page] = primary_slots[slot_index];
            std::cout << "  Page " << page << " (0x" << std::hex << (page * 0x4000) << "-0x" << ((page+1)*0x4000 -1) 
                      << ") -> Slot " << slot_index;
            // Show slot type
            if (primary_slots[slot_index]) {
                auto rom = std::dynamic_pointer_cast<RomSlot>(primary_slots[slot_index]);
                if (rom) std::cout << " (ROM)";
                else std::cout << " (RAM)";
            }
            std::cout << std::dec << std::endl;
        }
        map_count++;
    } else {
        // Just update mapping without logging
        for (int page = 0; page < 4; ++page) {
            int slot_index = (ppi_val >> (page * 2)) & 0x03;
            page_map[page] = primary_slots[slot_index];
        }
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
