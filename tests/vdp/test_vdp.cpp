#include <gtest/gtest.h>
#include "vdp/vdp.h"
#include "cpu/z80a.h"

class VDPTest : public ::testing::Test {
protected:
    VDP& vdp = VDP::getInstance();
    Z80A cpu;

    void SetUp() override {
        cpu.setMemoryReadCallback([](uint16_t) { return 0; });
        cpu.setMemoryWriteCallback([](uint16_t, uint8_t) { });
        vdp.setCPU(&cpu);
        vdp.reset();
    }
};

TEST_F(VDPTest, RegisterWrite_Test) {
    // Write 0x01 to Register 7 (Background color)
    vdp.writeControlPort(0x01); // First byte: Value
    vdp.writeControlPort(0x87); // Second byte: 0x80 | RegIndex (7)
    
    // Check if it was written correctly (can't check registers directly as they are private)
    // But we can check if it affects status if we find a way.
    // Let's check VRAM access instead.
}

TEST_F(VDPTest, VRAMWriteRead_Test) {
    // Set VRAM address to 0x1234 for write
    vdp.writeControlPort(0x34);
    vdp.writeControlPort(0x40 | 0x12); // Bit 6 set for write
    
    vdp.writeDataPort(0xAB);
    vdp.writeDataPort(0xCD);
    
    // Set VRAM address to 0x1234 for read
    vdp.writeControlPort(0x34);
    vdp.writeControlPort(0x00 | 0x12); // Bit 6 clear for read
    
    // First read should return the read-ahead buffer (which might be 0 after reset or the last written value depending on implementation)
    // Actually, setting the read address should trigger a read-ahead.
    uint8_t val1 = vdp.readDataPort();
    EXPECT_EQ(val1, 0xAB);
    
    uint8_t val2 = vdp.readDataPort();
    EXPECT_EQ(val2, 0xCD);
}

TEST_F(VDPTest, InterruptTrigger_Test) {
    vdp.reset();
    // Enable interrupts in Register 1
    vdp.writeControlPort(0x20); // Value: bit 5 set
    vdp.writeControlPort(0x81); // Register 1
    
    cpu.IFF1 = true;
    cpu.PC = 0x1000;
    
    // Force enough cycles to trigger interrupt
    vdp.update(60000); 
    
    EXPECT_EQ(cpu.PC, 0x0038); // Interrupt triggered
}
