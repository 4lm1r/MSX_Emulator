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
RamSlot::RamSlot(size_t size) { 
    buffer.resize(size, 0); 
    std::cout << "RamSlot created with size " << size << " bytes" << std::endl;
}
uint8_t RamSlot::read(uint16_t addr) const {
    if (addr < buffer.size()) return buffer[addr];
    return 0xFF;
}
void RamSlot::write(uint16_t addr, uint8_t value) {
    if (addr < buffer.size()) {
        buffer[addr] = value;
        // Log first few writes to verify
        static int write_log_count = 0;
        if (write_log_count < 10 && addr >= 0x8000) {
            std::cout << "RamSlot::write: addr=0x" << std::hex << addr 
                      << " value=0x" << (int)value << std::dec << std::endl;
            write_log_count++;
        }
    }
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
    // Default setup: Slot 0 = ROM, Slots 1,2,3 = RAM
    primary_slots[0] = std::make_shared<RomSlot>(64 * 1024);
    primary_slots[1] = std::make_shared<RamSlot>(64 * 1024);
    primary_slots[2] = std::make_shared<RamSlot>(64 * 1024);
    primary_slots[3] = std::make_shared<RamSlot>(64 * 1024);
    
    std::cout << "  Slot 0: ROM (64KB)" << std::endl;
    std::cout << "  Slot 1: RAM (64KB)" << std::endl;
    std::cout << "  Slot 2: RAM (64KB)" << std::endl;
    std::cout << "  Slot 3: RAM (64KB)" << std::endl;
    
    // Map according to MSX1 standard with RAM in pages 2 and 3
    // 0xF0 = 1111 0000: Page 0=0 (ROM), Page 1=0 (ROM), Page 2=3 (RAM), Page 3=3 (RAM)
    // This puts RAM at 0x8000-0xFFFF where the stack should be
    mapPrimarySlots(0xF0);
    std::cout << "Memory::reset: Mapped slots with ppi_val=0xF0" << std::endl;
}

uint8_t Memory::read(uint16_t addr) const {
    int page = (addr >> 14) & 0x03;
    // Log only first few reads and when page changes significantly
    static int read_count = 0;
    static uint16_t last_addr = 0xFFFF;
    static int last_page = -1;
    
    bool should_log = false;
    if (read_count < 10) {
        should_log = true;
        read_count++;
    } else if ((addr >> 8) != (last_addr >> 8)) { // Log when high byte changes
        should_log = true;
    }
    
    if (should_log) {
        std::cout << "MEM read: addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr 
                  << " page=" << page << " slot=" << ((current_ppi_val >> (page*2)) & 0x03);
        uint8_t val = (page_map[page]) ? page_map[page]->read(addr) : 0xFF;
        std::cout << " val=0x" << std::setw(2) << std::setfill('0') << (int)val << std::dec << std::endl;
        last_addr = addr;
        last_page = page;
        return val;
    }
    
    if (page_map[page]) return page_map[page]->read(addr);
    return 0xFF;
}

void Memory::write(uint16_t addr, uint8_t value) {
    int page = (addr >> 14) & 0x03;
    // Always log writes to RAM area (0x8000-0xFFFF)
    static bool log_ram_writes = true;
    if (addr >= 0x8000 && log_ram_writes) {
        std::cout << "RAM WRITE: addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr 
                  << " page=" << page << " slot=" << ((current_ppi_val >> (page*2)) & 0x03)
                  << " val=0x" << std::setw(2) << (int)value << std::dec << std::endl;
        // Stop logging after 20 writes to avoid flooding
        static int ram_write_count = 0;
        if (++ram_write_count >= 20) {
            log_ram_writes = false;
            std::cout << "RAM WRITE: Stopping logs to avoid flooding" << std::dec << std::endl;
        }
    }
    
    // Log first few writes regardless of address
    static int write_count = 0;
    if (write_count < 10) {
        std::cout << "MEM write: addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr 
                  << " page=" << page << " slot=" << ((current_ppi_val >> (page*2)) & 0x03)
                  << " val=0x" << std::setw(2) << (int)value << std::dec << std::endl;
        write_count++;
    }
    
    if (page_map[page]) {
        // Check if this is a ROM slot (read-only)
        auto rom = std::dynamic_pointer_cast<RomSlot>(page_map[page]);
        if (rom) {
            // ROM is read-only, ignore write
            static int rom_write_attempts = 0;
            if (rom_write_attempts < 10) {
                std::cout << "MEM write: Attempt to write to ROM at 0x" << std::hex << addr << " ignored" << std::dec << std::endl;
                rom_write_attempts++;
            }
            return;
        }
        // Write to RAM
        page_map[page]->write(addr, value);
        // Verify write for first few RAM writes
        static int verify_count = 0;
        if (verify_count < 10 && addr >= 0x8000) {
            uint8_t read_back = page_map[page]->read(addr);
            if (read_back != value) {
                std::cout << "MEM write: CRITICAL: write/read mismatch at 0x" << std::hex << addr 
                          << " wrote=0x" << (int)value << " read=0x" << (int)read_back 
                          << " page=" << page << " slot=" << ((current_ppi_val >> (page*2)) & 0x03)
                          << std::dec << std::endl;
            } else {
                std::cout << "MEM write: Verified RAM write at 0x" << std::hex << addr 
                          << " value=0x" << (int)value << std::dec << std::endl;
            }
            verify_count++;
        }
    } else {
        // No slot mapped for this page - this should not happen with proper PPI setup
        std::cout << "MEM write: ERROR: no slot mapped for page " << page 
                  << " at addr 0x" << std::hex << addr << std::dec << std::endl;
        // Fallback: write to Slot 3 RAM if address is in RAM area
        if (addr >= 0x8000 && primary_slots[3]) {
            std::cout << "MEM write: Falling back to Slot 3 RAM for addr 0x" << std::hex << addr << std::dec << std::endl;
            primary_slots[3]->write(addr, value);
        }
    }
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
    // Always log mappings (first 30)
    static int map_count = 0;
    if (map_count < 30) {
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
