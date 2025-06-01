#include <gtest/gtest.h>
#include "cpu/z80a.h"

class Z80ATest : public ::testing::Test {
protected:
    Z80A cpu;
};

TEST_F(Z80ATest, ResetInitializesRegisters_Test) {
    // Arrange: Reset the CPU
    cpu.reset();

    // Act: No additional action needed, reset was called

    // Assert: Check that registers and PC are initialized to 0
    EXPECT_EQ(cpu.A, 0);
    EXPECT_EQ(cpu.B, 0);
    EXPECT_EQ(cpu.PC, 0);
    // Add more assertions for other registers (C, D, E, H, L, F) if needed
    EXPECT_EQ(cpu.C, 0);
    EXPECT_EQ(cpu.D, 0);
    EXPECT_EQ(cpu.E, 0);
    EXPECT_EQ(cpu.H, 0);
    EXPECT_EQ(cpu.L, 0);
    EXPECT_EQ(cpu.F, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
