#include <iostream>
#include <vector>
#include <cstring>
#include <SDL2/SDL.h>
#include "cpu/z80a.h"
#include "vdp/vdp.h"
#include "memory/memory.h"
#include "ppi/ppi.h"
#include "keyboard/keyboard.h"
#include <fstream>
#include "debug.h"

int main(int argc, char* argv[]) {
    Z80A cpu;
    Memory& memory = Memory::getInstance();
    VDP& vdp = VDP::getInstance();
    PPI& ppi = PPI::getInstance();
    Keyboard& keyboard = Keyboard::getInstance();

    vdp.setCPU(&cpu);
    vdp.setDebugLogStream(debug_log);

    // Set CPU callbacks to use Memory class
    cpu.setMemoryReadCallback([&](uint16_t addr) { return memory.read(addr); });
    cpu.setMemoryWriteCallback([&](uint16_t addr, uint8_t val) { memory.write(addr, val); });

    // IO Callbacks
    cpu.setIOReadCallback([&](uint8_t port) -> uint8_t {
        if (port == 0x98) return vdp.readDataPort();
        if (port == 0x99) return vdp.readControlPort();
        if ((port & 0xFC) == 0xA8) return ppi.read(port & 0x03);
        return 0xFF;
    });
    cpu.setIOWriteCallback([&](uint8_t port, uint8_t val) {
        if (port == 0x98) vdp.writeDataPort(val);
        else if (port == 0x99) vdp.writeControlPort(val);
        else if ((port & 0xFC) == 0xA8) ppi.write(port & 0x03, val);
    });

    // PPI Slot Selection Callback
    ppi.setSlotSelectCallback([&](uint8_t slot_val) {
        memory.mapPrimarySlots(slot_val);
    });

    // SDL Initialization
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("MSX Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * 2, 192 * 2, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, 256, 192);

    // Try loading BIOS from relative path
    bool bios_loaded = memory.loadROM("roms/MSX.ROM", 0x0000);
    if (!bios_loaded) {
        bios_loaded = memory.loadROM("../roms/MSX.ROM", 0x0000);
    }
    
    if (!bios_loaded) {
        std::cerr << "Warning: Could not load roms/MSX.ROM. Emulator might not work correctly." << std::endl;
        // Fallback: simple test program
        // (This won't work well with slot-based memory unless we ensure it's in RAM or ROM slot)
    }

    cpu.PC = 0x0000;
    bool running = true;
    SDL_Event event;
    uint8_t screen_buffer[256 * 192 * 4];

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            keyboard.processEvent(event);
        }

        // Execute one frame worth of cycles (approx 59736 cycles for 60Hz MSX)
        int frame_cycles = 0;
        while (frame_cycles < 59736) {
            int cycles = cpu.execute();
            vdp.update(cycles);
            frame_cycles += cycles;
        }

        // Render VDP output
        vdp.render(screen_buffer, 256, 192);
        SDL_UpdateTexture(texture, nullptr, screen_buffer, 256 * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(16); // Approx 60 FPS
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
