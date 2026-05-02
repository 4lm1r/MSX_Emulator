#ifndef PSG_H
#define PSG_H

#include <cstdint>
#include <atomic>
#include <SDL2/SDL.h>

// AY-3-8910 Programmable Sound Generator
// Registers:
//  R0/R1   Channel A tone period (fine/coarse, 12-bit)
//  R2/R3   Channel B tone period
//  R4/R5   Channel C tone period
//  R6      Noise period (5-bit)
//  R7      Mixer: bits 0-2 tone disable A/B/C, bits 3-5 noise disable A/B/C
//  R8/R9/R10  Amplitude A/B/C (bits 0-3 level, bit 4 envelope mode)
//  R11/R12 Envelope period (fine/coarse)
//  R13     Envelope shape (CONT/ATT/ALT/HOLD)
//  R14/R15 I/O ports
class PSG {
public:
    static PSG& getInstance();
    void reset();
    void writeAddress(uint8_t addr);
    void writeData(uint8_t data);
    uint8_t readData();

    void initAudio();
    void closeAudio();

    // Call every CPU instruction with the elapsed cycle count
    void update(int cpu_cycles);

private:
    PSG();
    ~PSG();
    PSG(const PSG&)            = delete;
    PSG& operator=(const PSG&) = delete;

    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int RING_SIZE   = 4096;  // must be power-of-2

    // Registers
    uint8_t registers[16];
    uint8_t current_addr;

    // Tone counters and flip state for channels A(0) B(1) C(2)
    uint16_t tone_counter[3];
    bool     tone_flip[3];

    // Noise generator (17-bit LFSR)
    uint16_t noise_counter;
    uint32_t noise_lfsr;
    bool     noise_bit;

    // Envelope generator
    uint32_t env_counter;
    int      env_step;
    bool     env_attack;
    bool     env_holding;
    int      env_volume;

    // Fractional-cycle accumulators
    double ay_accumulator;
    double sample_accumulator;

    // Lock-free SPSC ring buffer (main thread writes, SDL callback reads)
    float            ring_buffer[RING_SIZE];
    std::atomic<int> write_pos{0};
    std::atomic<int> read_pos{0};

    SDL_AudioDeviceID audio_device;

    void  tickAY();
    void  stepEnvelope();
    void  pushSample(float s);
    float mixSample() const;

    static void audioCallback(void* userdata, uint8_t* stream, int len);

    // Hardware-measured logarithmic volume table (level 0..15)
    static const float VOLUME_TABLE[16];
};

#endif // PSG_H
