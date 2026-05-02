#include "psg.h"
#include <cstring>
#include <iostream>

// Logarithmic amplitude table derived from AY-3-8910 hardware measurements.
const float PSG::VOLUME_TABLE[16] = {
    0.0000f, 0.0137f, 0.0205f, 0.0307f,
    0.0455f, 0.0683f, 0.1020f, 0.1528f,
    0.2239f, 0.3260f, 0.4900f, 0.7260f,
    0.8350f, 0.8820f, 0.9985f, 1.0000f
};

PSG& PSG::getInstance() {
    static PSG instance;
    return instance;
}

PSG::PSG() : audio_device(0), ay_accumulator(0.0), sample_accumulator(0.0) {
    reset();
}

PSG::~PSG() {
    closeAudio();
}

void PSG::reset() {
    std::memset(registers, 0, sizeof(registers));
    registers[7]  = 0xFF;  // All tone/noise outputs disabled
    registers[14] = 0xFF;  // I/O port A: no joystick input
    registers[15] = 0xFF;  // I/O port B
    current_addr  = 0;

    std::memset(tone_counter, 0, sizeof(tone_counter));
    std::memset(tone_flip,    0, sizeof(tone_flip));

    noise_counter = 0;
    noise_lfsr    = 1;
    noise_bit     = false;

    env_counter = 0;
    env_step    = 0;
    env_attack  = false;
    env_holding = false;
    env_volume  = 0;

    ay_accumulator     = 0.0;
    sample_accumulator = 0.0;

    // Flush the ring buffer while the audio callback is silenced
    if (audio_device != 0) {
        SDL_LockAudioDevice(audio_device);
        write_pos.store(0, std::memory_order_relaxed);
        read_pos.store(0,  std::memory_order_relaxed);
        SDL_UnlockAudioDevice(audio_device);
    } else {
        write_pos.store(0, std::memory_order_relaxed);
        read_pos.store(0,  std::memory_order_relaxed);
    }
}

void PSG::writeAddress(uint8_t addr) {
    current_addr = addr & 0x0F;
}

void PSG::writeData(uint8_t data) {
    registers[current_addr] = data;

    // A write to R13 (envelope shape) resets the envelope generator
    if (current_addr == 13) {
        bool att       = (data >> 2) & 1;
        env_step       = 0;
        env_attack     = att;
        env_holding    = false;
        env_counter    = 0;
        env_volume     = att ? 0 : 15;
    }
}

uint8_t PSG::readData() {
    return registers[current_addr];
}

void PSG::initAudio() {
    SDL_AudioSpec want{}, got{};
    want.freq     = SAMPLE_RATE;
    want.format   = AUDIO_F32SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = audioCallback;
    want.userdata = this;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (audio_device == 0) {
        std::cerr << "PSG: SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_PauseAudioDevice(audio_device, 0);  // start playback
    std::cout << "PSG: Audio initialized at " << got.freq
              << " Hz, " << (int)got.channels << " ch" << std::endl;
}

void PSG::closeAudio() {
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
}

// Called every cpu.execute() tick (cpu_cycles = cycles returned by that call).
// AY-3-8910 runs at CPU_CLOCK/2 = 1,789,772.5 Hz (NTSC MSX).
void PSG::update(int cpu_cycles) {
    static constexpr double AY_CLOCKS_PER_SAMPLE = 1789772.5 / SAMPLE_RATE;

    ay_accumulator += cpu_cycles * 0.5;  // 1 AY clock = 2 CPU cycles

    while (ay_accumulator >= 1.0) {
        ay_accumulator -= 1.0;
        tickAY();

        sample_accumulator += 1.0;
        if (sample_accumulator >= AY_CLOCKS_PER_SAMPLE) {
            sample_accumulator -= AY_CLOCKS_PER_SAMPLE;
            pushSample(mixSample());
        }
    }
}

// Advance all AY-3-8910 counters by exactly one AY clock.
void PSG::tickAY() {
    // Tone channels A/B/C
    for (int ch = 0; ch < 3; ch++) {
        uint16_t period = ((registers[ch * 2 + 1] & 0x0F) << 8) | registers[ch * 2];
        if (period == 0) period = 1;
        if (++tone_counter[ch] >= period) {
            tone_counter[ch] = 0;
            tone_flip[ch]    = !tone_flip[ch];
        }
    }

    // Noise generator (5-bit period, 17-bit polynomial LFSR)
    {
        uint8_t np = registers[6] & 0x1F;
        if (np == 0) np = 1;
        if (++noise_counter >= np) {
            noise_counter = 0;
            bool fb    = ((noise_lfsr ^ (noise_lfsr >> 2)) & 1) != 0;
            noise_lfsr = (noise_lfsr >> 1) | (fb ? 0x10000u : 0u);
            noise_bit  = noise_lfsr & 1;
        }
    }

    // Envelope generator
    {
        uint16_t ep = ((uint16_t)registers[12] << 8) | registers[11];
        if (ep == 0) ep = 1;
        if (++env_counter >= ep) {
            env_counter = 0;
            stepEnvelope();
        }
    }
}

// Advance the envelope by one step.
// Shape bits (R13): bit3=CONT  bit2=ATT  bit1=ALT  bit0=HOLD
void PSG::stepEnvelope() {
    if (env_holding) return;

    const uint8_t shape = registers[13] & 0x0F;
    const bool cont = (shape >> 3) & 1;
    const bool alt  = (shape >> 1) & 1;
    const bool hold = (shape >> 0) & 1;

    ++env_step;

    if (env_step >= 16) {
        env_step = 0;

        if (!cont) {
            // Single-shot: silence after the first ramp
            env_volume  = 0;
            env_holding = true;
            return;
        }

        if (hold) {
            // Hold at the value that starts the NEXT phase
            // (next direction determined by ALT bit)
            bool next_attack = alt ? !env_attack : env_attack;
            env_volume  = next_attack ? 15 : 0;
            env_holding = true;
            return;
        }

        if (alt) env_attack = !env_attack;
    }

    env_volume = env_attack ? env_step : (15 - env_step);
}

// Mix current channel outputs into a single normalised [-1, +1] sample.
float PSG::mixSample() const {
    const uint8_t mixer = registers[7];
    float output = 0.0f;

    for (int ch = 0; ch < 3; ch++) {
        const bool tone_off  = (mixer >> ch)       & 1;  // 1 = disabled
        const bool noise_off = (mixer >> (ch + 3)) & 1;

        // Channel is high when: (tone is high OR tone is disabled)
        //                  AND  (noise is high OR noise is disabled)
        const bool ch_high = (tone_flip[ch] || tone_off) && (noise_bit || noise_off);

        const uint8_t amp_reg = registers[8 + ch];
        const float amplitude = VOLUME_TABLE[
            (amp_reg & 0x10) ? (env_volume & 0x0F) : (amp_reg & 0x0F)
        ];

        // Use ±amplitude to remove DC offset (square wave centred at 0)
        output += ch_high ? amplitude : -amplitude;
    }

    // Three channels, each ±1.0 max → normalise to [-1, +1]
    return output / 3.0f;
}

void PSG::pushSample(float s) {
    const int wp      = write_pos.load(std::memory_order_relaxed);
    const int next_wp = (wp + 1) & (RING_SIZE - 1);
    if (next_wp != read_pos.load(std::memory_order_acquire)) {
        ring_buffer[wp] = s;
        write_pos.store(next_wp, std::memory_order_release);
    }
    // Buffer full: drop sample rather than blocking
}

void PSG::audioCallback(void* userdata, uint8_t* stream, int len) {
    auto* psg     = static_cast<PSG*>(userdata);
    auto* out     = reinterpret_cast<float*>(stream);
    int   samples = len / sizeof(float);

    for (int i = 0; i < samples; i++) {
        const int rp = psg->read_pos.load(std::memory_order_relaxed);
        const int wp = psg->write_pos.load(std::memory_order_acquire);

        if (rp != wp) {
            out[i] = psg->ring_buffer[rp];
            psg->read_pos.store((rp + 1) & (RING_SIZE - 1), std::memory_order_release);
        } else {
            out[i] = 0.0f;  // Buffer underrun: output silence
        }
    }
}
