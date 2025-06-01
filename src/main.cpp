#include <iostream>
#include <vector>
#include <cstring>
#include <SDL2/SDL.h>
#include "cpu/z80a.h"
#include "vdp/vdp.h"
#include "memory/memory.h"
#include <fstream>
#include "debug.h"

static uint8_t test_memory[0x10000];
static uint16_t memory_index = 0;
static uint8_t current_slot = 0;
static uint16_t slot_base[2] = {0x0000, 0x4000};
static bool slot_logged = false;

uint8_t mock_memory_read(uint16_t addr) {
    int slot = 0;
    current_slot = slot;
    uint16_t offset = addr - slot_base[slot];
    if (offset >= 0x10000) {
        debug_log << "Invalid offset 0x" << std::hex << offset << " for addr 0x" << addr << std::dec << std::endl;
        return 0xFF;
    }
    return test_memory[offset];
}

void mock_memory_write(uint16_t addr, uint8_t value) {
    int slot = 0;
    current_slot = slot;
    uint16_t offset = addr - slot_base[slot];
    if (offset >= 0x10000) {
        debug_log << "Invalid offset 0x" << std::hex << offset << " for addr 0x" << addr << std::dec << std::endl;
        return;
    }
    test_memory[offset] = value;
}

void setup_memory(const std::vector<uint8_t>& data, uint16_t start_addr = 0x0000) {
    memory_index = start_addr;
    if (memory_index + data.size() > 0x10000) {
        debug_log << "Memory overflow: data size exceeds 64KB" << std::endl;
        return;
    }
    for (uint8_t byte : data) {
        test_memory[memory_index++] = byte;
    }
}

int main(int argc, char* argv[]) {
    Z80A cpu;
    VDP& vdp = VDP::getInstance();
    vdp.setCPU(&cpu);
    vdp.setDebugLogStream(debug_log);
    Memory memory;

    cpu.setMemoryReadCallback(std::function<uint8_t(uint16_t)>(mock_memory_read));
    cpu.setMemoryWriteCallback(std::function<void(uint16_t, uint8_t)>(mock_memory_write));
    cpu.setIOReadCallback([](uint8_t port) -> uint8_t {
        VDP& vdp = VDP::getInstance();
        if (port == 0x98) {
            debug_log << "Reading VDP data port (0x98)" << std::endl;
            return vdp.readDataPort();
        }
        if (port == 0x99) {
            debug_log << "Reading VDP control port (0x99)" << std::endl;
            return vdp.readControlPort();
        }
        debug_log << "Reading unmapped port 0x" << std::hex << (int)port << std::dec << ": returning 0xFF" << std::endl;
        return 0xFF;
    });
    cpu.setIOWriteCallback([](uint8_t port, uint8_t value) {
        VDP& vdp = VDP::getInstance();
        if (port == 0x98) {
            debug_log << "Writing VDP data port (0x98): 0x" << std::hex << (int)value << std::dec << std::endl;
            vdp.writeDataPort(value);
        }
        else if (port == 0x99) {
            debug_log << "Writing VDP control port (0x99): 0x" << std::hex << (int)value << std::dec << std::endl;
            vdp.writeControlPort(value);
        }
        else {
            debug_log << "Writing unmapped port 0x" << std::hex << (int)port << ": 0x" << (int)value << std::dec << std::endl;
        }
    });

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        debug_log << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "MSX Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        256 * 2,
        192 * 2,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        debug_log << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        debug_log << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_BGRA8888,
        SDL_TEXTUREACCESS_STREAMING,
        256, 192
    );
    if (!texture) {
        debug_log << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    memset(test_memory, 0xFF, sizeof(test_memory));

    bool bios_loaded = false;

    std::ifstream bios_file("/home/abr/PROJECTS/C++Projects/AdvancedC++/MSXEmulator/roms/MSXBR.rom", std::ios::binary);
    if (!bios_file) {
        debug_log << "Failed to load BIOS (msx_bios.rom). Using fallback test program." << std::endl;
    } else {
        bios_file.read(reinterpret_cast<char*>(&test_memory[0]), 0x8000);
        size_t bios_size = bios_file.gcount();
        debug_log << "Loaded BIOS (" << bios_size << " bytes) at address 0x0" << std::endl;
        bios_file.close();
        bios_loaded = true;
        // Clear the remaining memory after the BIOS
        if (bios_size < sizeof(test_memory)) {
            memset(&test_memory[bios_size], 0xFF, sizeof(test_memory) - bios_size);
        }
        // Log the ISR at 0x0038
        debug_log << "ISR at 0x0038-0x0041: ";
        for (uint16_t addr = 0x0038; addr <= 0x0041; addr++) {
            debug_log << std::hex << "0x" << (int)test_memory[addr] << " ";
        }
        debug_log << std::dec << std::endl;
    }

    cpu.PC = 0x0000;
    debug_log << "Starting at BIOS entry point: 0x0000" << std::endl;

    bool rom_loaded = false;

    debug_log << "Starting stack initialization" << std::endl;
    int write_count = 0;
    uint16_t addr = 0xF380;
    while (write_count < 0xC80) {
        mock_memory_write(addr, 0xFF);
        addr++;
        write_count++;
    }
    debug_log << "Finished stack initialization, wrote " << write_count << " bytes" << std::endl;
    debug_log << "Stack at 0xfffc-0xffff: 0x" << std::hex << (int)mock_memory_read(0xfffc)
              << " 0x" << (int)mock_memory_read(0xfffd)
              << " 0x" << (int)mock_memory_read(0xfffe)
              << " 0x" << (int)mock_memory_read(0xffff) << std::dec << std::endl;

    cpu.IFF1 = true;
    cpu.IFF2 = true;
    debug_log << "Interrupts forcibly enabled: IFF1=" << cpu.IFF1 << ", IFF2=" << cpu.IFF2 << std::endl;

    if (!bios_loaded && !rom_loaded) {
        debug_log << "No BIOS or ROM loaded. Using hardcoded test program." << std::endl;
        cpu.reset();
        setup_memory({
            0x3E, 0xE0, 0xD3, 0x99,
            0x3E, 0x81, 0xD3, 0x99,
            0x3E, 0x06, 0xD3, 0x99,
            0x3E, 0x82, 0xD3, 0x99,
            0x3E, 0x80, 0xD3, 0x99,
            0x3E, 0x83, 0xD3, 0x99,
            0x3E, 0x00, 0xD3, 0x99,
            0x3E, 0x84, 0xD3, 0x99,
            0x3E, 0x36, 0xD3, 0x99,
            0x3E, 0x85, 0xD3, 0x99,
            0x3E, 0x1F, 0xD3, 0x99,
            0x3E, 0x86, 0xD3, 0x99,
            0x3E, 0x05,
            0xC6, 0x03,
            0xD6, 0x02,
            0xE6, 0x0F,
            0x06, 0x04,
            0xB0,
            0xEE, 0x01,
            0x07,
            0x0F,
            0xFE, 0x07,
            0x28, 0x02,
            0x3E, 0x00,
            0x10, 0xF2
        });
        for (int i = 0; i < 38; i++) {
            int cycles = cpu.execute();
            vdp.update(cycles);
        }
    }

    debug_log << "Memory at 0xc3c before execution: 0x" << std::hex << (int)mock_memory_read(0xc3c) << "\n";

    bool running = true;
    SDL_Event event;
    const uint32_t WIDTH = 256;
    const uint32_t HEIGHT = 192;
    uint8_t buffer[WIDTH * HEIGHT * 4];
    int sprite_x = 100;
    int sprite_y = 50;
    const int SPRITE_SPEED = 2;
    const uint64_t cycles_per_frame = 2280;
    uint64_t total_cycles = 0;
    uint64_t frame_cycles = 0;
    debug_log << "Memory at 0xc3c before execution: 0x" << std::hex << (int)mock_memory_read(0xc3c) << "\n";

    while (running) {
        frame_cycles = 0;
        debug_log << "Starting new frame, frame_cycles reset to " << frame_cycles << std::endl;
        while (frame_cycles < cycles_per_frame) {
            uint16_t current_pc = cpu.PC;
            uint8_t opcode = mock_memory_read(current_pc);
            debug_log << "Checking PC: 0x" << std::hex << current_pc << ", opcode: 0x" << (int)opcode << "\n";
            if (current_pc == 0x3f) {
                debug_log << "JP target bytes at 0x40-0x41: 0x" << std::hex << (int)mock_memory_read(0x40)
                          << " 0x" << (int)mock_memory_read(0x41) << "\n";
            }
            int cycles = cpu.execute();
            debug_log << "CPU executed, cycles returned: " << cycles << "\n";
            vdp.update(cycles);
            frame_cycles += cycles;
            total_cycles += cycles;
            if (total_cycles >= cycles_per_frame) {
        total_cycles -= cycles_per_frame;
        // Handle frame logic
    }
            
            debug_log << "Executed " << cycles << " cycles, frame_cycles=" << frame_cycles
                      << ", total_cycles=" << total_cycles << "\n";
        }

        if (frame_cycles >= cycles_per_frame) {
            debug_log << "Frame completed, total_cycles=" << total_cycles << "\n";
        } else {
            debug_log << "Frame loop exited early, frame_cycles=" << frame_cycles << ", total_cycles=" << total_cycles << "\n";
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                debug_log << "SDL event triggered exit: " << (event.type == SDL_QUIT ? "SDL_QUIT" : "ESCAPE key") << "\n";
                running = false;
            }
        }

        if (!running) {
            debug_log << "Main loop exiting, running=" << (running ? "true" : "false") << "\n";
            break;
        }

        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        if (keystate[SDL_SCANCODE_UP]) {
            sprite_y -= SPRITE_SPEED;
            if (sprite_y < 0) sprite_y = 0;
        }
        if (keystate[SDL_SCANCODE_DOWN]) {
            sprite_y += SPRITE_SPEED;
            if (sprite_y > HEIGHT - 8) sprite_y = HEIGHT - 8;
        }
        if (keystate[SDL_SCANCODE_LEFT]) {
            sprite_x -= SPRITE_SPEED;
            if (sprite_x < 0) sprite_x = 0;
        }
        if (keystate[SDL_SCANCODE_RIGHT]) {
            sprite_x += SPRITE_SPEED;
            if (sprite_x > WIDTH - 8) sprite_x = WIDTH - 8;
        }

        std::memset(buffer, 0, sizeof(buffer));
        vdp.render(buffer, WIDTH, HEIGHT);

        uint32_t first_pixel = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
        debug_log << "Buffer first pixel (BGRA): 0x" << std::hex << first_pixel << std::dec << std::endl;

        SDL_UpdateTexture(texture, nullptr, buffer, WIDTH * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    debug_log << "Cleaning up SDL resources\n";
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    debug_log.close();

    return 0;
}
