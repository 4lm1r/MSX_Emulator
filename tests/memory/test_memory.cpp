#include <gtest/gtest.h>
#include "memory/memory.h"
#include <fstream>

class MemoryTest : public ::testing::Test {
protected:
    Memory& memory = Memory::getInstance();

    void SetUp() override {
        memory.reset();
    }
};

TEST_F(MemoryTest, WriteProtection_Test) {
    // Slot 0 is ROM, Slot 1 is RAM
    memory.mapPrimarySlots(0x00); // All pages to Slot 0
    
    // Try writing to Slot 0 (ROM)
    memory.write(0x1000, 0xAB);
    EXPECT_EQ(memory.read(0x1000), 0x00);
    
    // Switch Page 0 to Slot 1 (RAM)
    memory.mapPrimarySlots(0x01); // Page 0 = Slot 1, Page 1,2,3 = Slot 0
    memory.write(0x1000, 0xCD);
    EXPECT_EQ(memory.read(0x1000), 0xCD);
    
    // Switch back to Slot 0
    memory.mapPrimarySlots(0x00);
    EXPECT_EQ(memory.read(0x1000), 0x00); // Should be original value
}

TEST_F(MemoryTest, SlotSwitching_Test) {
    // Map Page 0 to Slot 1 (RAM), Page 1 to Slot 0 (ROM)
    memory.mapPrimarySlots(0x01); // 00 00 00 01 (Binary)
    
    memory.write(0x1000, 0xAA); // Page 0
    memory.write(0x5000, 0xBB); // Page 1
    
    EXPECT_EQ(memory.read(0x1000), 0xAA);
    EXPECT_EQ(memory.read(0x5000), 0x00); // Page 1 is Slot 0 (ROM)
}

TEST_F(MemoryTest, LoadROM_Test) {
    // Create a temporary dummy ROM file
    const char* filename = "test_dummy.rom";
    std::ofstream out(filename, std::ios::binary);
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    out.write(reinterpret_cast<char*>(data), sizeof(data));
    out.close();
    
    bool loaded = memory.loadROM(filename, 0x1000);
    EXPECT_TRUE(loaded);
    EXPECT_EQ(memory.read(0x1000), 0x01);
    EXPECT_EQ(memory.read(0x1003), 0x04);
    
    std::remove(filename);
}
