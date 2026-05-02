#include <gtest/gtest.h>
#include "cpu/z80a.h"
#include <vector>

class Z80ATest : public ::testing::Test {
protected:
    Z80A cpu;
    std::vector<uint8_t> mock_mem;

    void SetUp() override {
        mock_mem.assign(0x10000, 0);
        cpu.setMemoryReadCallback([this](uint16_t addr) { return mock_mem[addr]; });
        cpu.setMemoryWriteCallback([this](uint16_t addr, uint8_t val) { mock_mem[addr] = val; });
        cpu.reset();
    }

    void loadProgram(const std::vector<uint8_t>& prog, uint16_t addr = 0x0000) {
        for (size_t i = 0; i < prog.size(); ++i) {
            mock_mem[addr + i] = prog[i];
        }
    }
};

TEST_F(Z80ATest, ResetInitializesRegisters_Test) {
    EXPECT_EQ(cpu.A, 0);
    EXPECT_EQ(cpu.PC, 0);
    EXPECT_EQ(cpu.F, 0);
}

TEST_F(Z80ATest, LDA_n_Test) {
    loadProgram({0x3E, 0x42}); // LD A, 0x42
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x42);
    EXPECT_EQ(cpu.PC, 2);
}

TEST_F(Z80ATest, ADDA_n_Test) {
    cpu.A = 0x10;
    loadProgram({0xC6, 0x05}); // ADD A, 0x05
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x15);
    EXPECT_FALSE(cpu.F & Z80A::C_BIT);
}

TEST_F(Z80ATest, SUBA_n_Test) {
    cpu.A = 0x10;
    loadProgram({0xD6, 0x05}); // SUB 0x05
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x0B);
    EXPECT_TRUE(cpu.F & Z80A::N_BIT);
}

TEST_F(Z80ATest, XORA_Test) {
    cpu.A = 0xFF;
    loadProgram({0xAF}); // XOR A
    cpu.execute();
    EXPECT_EQ(cpu.A, 0);
    EXPECT_TRUE(cpu.F & Z80A::Z_BIT);
}

TEST_F(Z80ATest, JP_nn_Test) {
    loadProgram({0xC3, 0x50, 0x00}); // JP 0x0050
    cpu.execute();
    EXPECT_EQ(cpu.PC, 0x0050);
}

TEST_F(Z80ATest, CALL_RET_Test) {
    loadProgram({0xCD, 0x20, 0x00}); // CALL 0x0020 at 0x0000
    cpu.SP = 0xFFFF;
    cpu.execute();
    EXPECT_EQ(cpu.PC, 0x0020);
    EXPECT_EQ(cpu.SP, 0xFFFD);
    EXPECT_EQ(mock_mem[0xFFFD], 0x03); // Return address low byte
    EXPECT_EQ(mock_mem[0xFFFE], 0x00); // Return address high byte
}

TEST_F(Z80ATest, FlagHalfCarryAdd_Test) {
    cpu.A = 0x0F;
    loadProgram({0xC6, 0x01}); // ADD A, 0x01
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x10);
    EXPECT_TRUE(cpu.F & Z80A::H_BIT);
}

TEST_F(Z80ATest, FlagOverflowAdd_Test) {
    cpu.A = 0x7F;
    loadProgram({0xC6, 0x01}); // ADD A, 0x01
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x80);
    EXPECT_TRUE(cpu.F & Z80A::P_BIT); // V flag is same as P bit in ADD
}

TEST_F(Z80ATest, FlagCarryAdd_Test) {
    cpu.A = 0xFF;
    loadProgram({0xC6, 0x01}); // ADD A, 0x01
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x00);
    EXPECT_TRUE(cpu.F & Z80A::C_BIT);
    EXPECT_TRUE(cpu.F & Z80A::Z_BIT);
}

TEST_F(Z80ATest, INC_Flags_Test) {
    cpu.B = 0x0F;
    loadProgram({0x04}); // INC B
    cpu.execute();
    EXPECT_EQ(cpu.B, 0x10);
    EXPECT_TRUE(cpu.F & Z80A::H_BIT);
    EXPECT_FALSE(cpu.F & Z80A::C_BIT); // INC does not affect Carry
}

TEST_F(Z80ATest, DEC_Flags_Test) {
    cpu.B = 0x10;
    loadProgram({0x05}); // DEC B
    cpu.execute();
    EXPECT_EQ(cpu.B, 0x0F);
    EXPECT_TRUE(cpu.F & Z80A::H_BIT); // Borrow from bit 4
    EXPECT_TRUE(cpu.F & Z80A::N_BIT); // N flag set for SUB/DEC
}

TEST_F(Z80ATest, JR_Test) {
    loadProgram({0x18, 0x05}); // JR +5
    cpu.execute();
    EXPECT_EQ(cpu.PC, 0x0007);
}

TEST_F(Z80ATest, Interrupt_Test) {
    cpu.IFF1 = true;
    cpu.PC = 0x1234;
    cpu.SP = 0xFFFF;
    cpu.triggerInterrupt();
    EXPECT_EQ(cpu.PC, 0x0038);
    EXPECT_EQ(cpu.SP, 0xFFFD);
    EXPECT_EQ(mock_mem[0xFFFD], 0x34);
    EXPECT_EQ(mock_mem[0xFFFE], 0x12);
    EXPECT_FALSE(cpu.IFF1);
}

TEST_F(Z80ATest, LDIR_Test) {
    cpu.setHL(0x1000);
    cpu.setDE(0x2000);
    cpu.setBC(0x0003);
    mock_mem[0x1000] = 0xAA;
    mock_mem[0x1001] = 0xBB;
    mock_mem[0x1002] = 0xCC;
    
    loadProgram({0xED, 0xB0}); // LDIR
    
    // BC = 3, so should take 3 executions
    cpu.execute(); 
    EXPECT_EQ(mock_mem[0x2000], 0xAA);
    EXPECT_EQ(cpu.getBC(), 2);
    EXPECT_EQ(cpu.PC, 0x0000); // PC should be back at start of LDIR

    cpu.execute();
    EXPECT_EQ(mock_mem[0x2001], 0xBB);
    EXPECT_EQ(cpu.getBC(), 1);
    EXPECT_EQ(cpu.PC, 0x0000);

    cpu.execute();
    EXPECT_EQ(mock_mem[0x2002], 0xCC);
    EXPECT_EQ(cpu.getBC(), 0);
    EXPECT_EQ(cpu.PC, 0x0002); // PC should now point past LDIR
}

TEST_F(Z80ATest, DEC_Flags_Zero_Test) {
    cpu.B = 0x01;
    loadProgram({0x05}); // DEC B
    cpu.execute();
    EXPECT_EQ(cpu.B, 0);
    EXPECT_TRUE(cpu.F & Z80A::Z_BIT);
    EXPECT_FALSE(cpu.F & Z80A::S_BIT);
}

TEST_F(Z80ATest, ADD_Flags_Sign_Test) {
    cpu.A = 0x00;
    loadProgram({0xC6, 0x81}); // ADD A, 0x81
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x81);
    EXPECT_TRUE(cpu.F & Z80A::S_BIT);
}

TEST_F(Z80ATest, JP_Condition_Test) {
    cpu.F = 0; // Zero flag clear
    loadProgram({0x20, 0x05}); // JR NZ, 0x05
    cpu.execute();
    EXPECT_EQ(cpu.PC, 0x0007);
    
    cpu.reset();
    cpu.F = Z80A::Z_BIT; // Zero flag set
    loadProgram({0x20, 0x05}); // JR NZ, 0x05
    cpu.execute();
    EXPECT_EQ(cpu.PC, 0x0002); // No jump
}

TEST_F(Z80ATest, IX_LD_Test) {
    loadProgram({0xDD, 0x21, 0x34, 0x12}); // LD IX, 0x1234
    cpu.execute();
    EXPECT_EQ(cpu.IX, 0x1234);
}

TEST_F(Z80ATest, RST_Test) {
    loadProgram({0xCF}); // RST 08h at 0x0000
    cpu.SP = 0xFFFF;
    cpu.execute();
    EXPECT_EQ(cpu.PC, 0x0008);
    EXPECT_EQ(cpu.SP, 0xFFFD);
}

TEST_F(Z80ATest, RLCA_Test) {
    cpu.A = 0x81;
    loadProgram({0x07}); // RLCA
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x03);
    EXPECT_TRUE(cpu.F & Z80A::C_BIT);
}

TEST_F(Z80ATest, ADC_Test) {
    cpu.A = 0x01;
    cpu.F = Z80A::C_BIT;
    loadProgram({0x88}); // ADC A, B (B=0)
    cpu.B = 0x02;
    cpu.execute();
    EXPECT_EQ(cpu.A, 0x04); // 1 + 2 + carry(1)
}

