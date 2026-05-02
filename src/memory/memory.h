#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

// Base class for physical memory blocks (ROM or RAM)
class MemoryBlock {
public:
    virtual ~MemoryBlock() = default;
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void write(uint16_t addr, uint8_t value) = 0;
};

class RomBlock : public MemoryBlock {
public:
    RomBlock(size_t size);
    uint8_t read(uint16_t addr) const override;
    void write(uint16_t addr, uint8_t value) override;
    bool load(const std::string& filename, uint16_t offset = 0);
private:
    std::vector<uint8_t> buffer;
};

class RamBlock : public MemoryBlock {
public:
    RamBlock(size_t size);
    uint8_t read(uint16_t addr) const override;
    void write(uint16_t addr, uint8_t value) override;
private:
    std::vector<uint8_t> buffer;
};

// Represents an MSX Primary Slot, which may contain subslots
struct PrimarySlot {
    bool expanded = false;
    uint8_t secondary_slot_register = 0;
    std::shared_ptr<MemoryBlock> subslots[4]; // Subslots 0-3
};

class Memory {
public:
    static Memory& getInstance();
    void reset();

    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t value);

    // PPI Port A updates the primary mapping for all 4 pages
    void mapPrimarySlots(uint8_t ppi_val);

    // Helper to load BIOS
    bool loadROM(const std::string& filename, uint16_t addr);

    // Load cartridge ROM into slot 1 (pages 1–2, 0x4000–0xBFFF)
    bool loadCartridge(const std::string& filename);

private:
    Memory();
    
    PrimarySlot slots[4];
    uint8_t current_ppi_val;

    // Internal mapping helpers
    uint8_t getActiveSecondarySlot(int primary_slot, int page) const;
};

#endif
