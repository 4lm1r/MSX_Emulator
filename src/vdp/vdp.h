#ifndef VDP_H
#define VDP_H

#include <cstdint>
#include <fstream>

class Z80A; // Forward declaration

class VDP {
public:
    static VDP& getInstance();
    void setCPU(Z80A* cpu);
    void reset();
    void update(int cycles);
    uint8_t readDataPort();
    void writeDataPort(uint8_t value);
    uint8_t readControlPort();
    void writeControlPort(uint8_t value);
    void render(uint8_t* buffer, uint32_t width, uint32_t height);
    void setDebugLogStream(std::ofstream& log_stream); // Add setter for debug log stream

private:
    VDP();
    static constexpr int VRAM_SIZE = 0x4000; // 16KB
    static constexpr int NUM_REGISTERS = 8;
    static constexpr int CYCLES_PER_FRAME = 59736; // 3579545 / 59.92 Hz 
    uint8_t vram[VRAM_SIZE];
    uint8_t registers[NUM_REGISTERS];
    uint8_t status;
    uint16_t vram_addr;
    uint8_t temp_addr;
    bool is_second_byte;
    bool write_mode;
    uint8_t read_buffer;
    int cycle_counter;
    bool interrupt_triggered;
    Z80A* cpu;
    std::ofstream* debug_log; // Pointer to debug log stream
};

#endif // VDP_H
