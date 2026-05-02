#include "vdp.h"
#include "z80a.h"
#include <cstring>

VDP& VDP::getInstance() {
    static VDP instance;
    return instance;
}

VDP::VDP() : cpu(nullptr), debug_log(nullptr), status(0), vram_addr(0), read_buffer(0), 
            is_second_byte(false), write_mode(false), cycle_counter(0), interrupt_triggered(false) {
        std::memset(vram, 0, VRAM_SIZE);
        std::memset(registers, 0, sizeof(registers));
}

void VDP::setCPU(Z80A* cpu_ptr) {
    cpu = cpu_ptr;
}

void VDP::setDebugLogStream(std::ofstream& log_stream) {
    debug_log = &log_stream;
}

void VDP::reset() {
    std::memset(vram, 0, VRAM_SIZE);
    std::memset(registers, 0, NUM_REGISTERS);
    status = 0;
    vram_addr = 0;
    temp_addr = 0;
    is_second_byte = false;
    read_buffer = 0;
    cycle_counter = 0;
    interrupt_triggered = false;
    write_mode = false; // Ensure default state
    // Set default mode: Graphic Mode 2
    registers[0] = 0x00; // External video off, 16K VRAM
    registers[1] = 0x80; // Enable display, 16x16 sprites, interrupts DISABLED initially (bit 5 cleared)
}

void VDP::update(int cycles) {
    cycle_counter += cycles;
    if (cycle_counter >= CYCLES_PER_FRAME) {
        cycle_counter -= CYCLES_PER_FRAME;
        status |= 0x80;
        interrupt_triggered = false;
    }

    if ((status & 0x80) && (registers[1] & 0x20) && cpu && !interrupt_triggered) {
        if (cpu->IFF1) {
            cpu->triggerInterrupt();
            interrupt_triggered = true;
        }
    }
}

uint8_t VDP::readDataPort() {
    if (write_mode) {
        if (debug_log) {
            *debug_log << "readDataPort: Attempted read in write mode at addr=0x" << std::hex << vram_addr << std::dec << std::endl;
        }
        return read_buffer;
    }
    if (vram_addr >= VRAM_SIZE) {
        if (debug_log) {
            *debug_log << "readDataPort: Invalid vram_addr=0x" << std::hex << vram_addr << std::dec << std::endl;
        }
        return read_buffer;
    }
    uint8_t data = read_buffer;
    read_buffer = vram[vram_addr];
    if (debug_log) {
        *debug_log << "readDataPort: addr=0x" << std::hex << vram_addr << ", value=0x" << (int)read_buffer << std::dec << std::endl;
    }
    vram_addr = (vram_addr + 1) & (VRAM_SIZE - 1);
    return data;
}

void VDP::writeDataPort(uint8_t value) {
    if (!write_mode) {
        if (debug_log) {
            *debug_log << "writeDataPort: Attempted write in read mode at addr=0x" << std::hex << vram_addr << std::dec << std::endl;
        }
        return;
    }
    if (vram_addr >= VRAM_SIZE) {
        if (debug_log) {
            *debug_log << "writeDataPort: Invalid vram_addr=0x" << std::hex << vram_addr << std::dec << std::endl;
        }
        return;
    }
    vram[vram_addr] = value;
    read_buffer = value;
    if (debug_log) {
        *debug_log << "writeDataPort: addr=0x" << std::hex << vram_addr << ", value=0x" << (int)value << std::dec << std::endl;
    }
    vram_addr = (vram_addr + 1) & (VRAM_SIZE - 1); // Auto-increment
}

uint8_t VDP::readControlPort() {
    uint8_t temp = status;
    status &= 0x1F; 
    interrupt_triggered = false;
    is_second_byte = false;
    return temp;
}

void VDP::writeControlPort(uint8_t value) {
    if (!is_second_byte) {
        temp_addr = value;
        is_second_byte = true;
        if (debug_log) {
            *debug_log << "writeControlPort: First byte, temp_addr=0x" << std::hex << (int)temp_addr << std::dec << std::endl;
        }
    } else {
        is_second_byte = false;
        if (value & 0x80) {
            // Register write
            uint8_t reg = value & 0x07; // Register number (0-7)
            registers[reg] = temp_addr; // Value from first byte
            if (debug_log) {
                *debug_log << "writeControlPort: Register write: reg=" << (int)reg << ", value=0x" << std::hex << (int)temp_addr << std::dec << std::endl;
                *debug_log << "Writing to VDP control port 0x99 (Register): 0x" << std::hex << (int)value << " at PC 0x" << cpu->PC << std::dec << std::endl;
            }
        } else {
            // VRAM address setup
            uint16_t addr = ((value & 0x3F) << 8) | temp_addr; // 14-bit address
            vram_addr = addr & 0x3FFF;
            write_mode = (value & 0x40) != 0; // Bit 6: 1 = write, 0 = read
            
            if (!write_mode) {
                // Read-ahead: when setting read address, immediately load read buffer
                read_buffer = vram[vram_addr];
                vram_addr = (vram_addr + 1) & 0x3FFF;
            }

            if (debug_log) {
                *debug_log << "writeControlPort: VRAM addr setup: vram_addr=0x" << std::hex << vram_addr << " (write=" << (write_mode ? "true" : "false") << ")" << std::dec << std::endl;
                *debug_log << "Writing to VDP control port 0x99 (VRAM Addr): 0x" << std::hex << (int)value << " at PC 0x" << cpu->PC << std::dec << std::endl;
            }
        }
    }
}

void VDP::render(uint8_t* buffer, uint32_t width, uint32_t height) {
    if (!(registers[1] & 0x40)) {
        if (buffer) std::memset(buffer, 0, width * height * 4);
        return;
    }

    bool m1 = (registers[1] >> 4) & 1;
    bool m2 = (registers[1] >> 3) & 1;
    bool m3 = (registers[0] >> 1) & 1;

    // Correct TMS9918A palette in 0xAARRGGBB layout
    const uint32_t palette[16] = {
        0x00000000, // 0:  Transparent
        0xFF000000, // 1:  Black
        0xFF3EB849, // 2:  Medium Green
        0xFF74D07D, // 3:  Light Green
        0xFF5955E0, // 4:  Dark Blue
        0xFF8076F1, // 5:  Light Blue
        0xFFB95E51, // 6:  Dark Red
        0xFF65DBEF, // 7:  Cyan
        0xFFDB6559, // 8:  Medium Red
        0xFFFF897D, // 9:  Light Red
        0xFFCCC35E, // 10: Dark Yellow
        0xFFDED087, // 11: Light Yellow
        0xFF3AA241, // 12: Dark Green
        0xFFB766B5, // 13: Magenta
        0xFFCCCCCC, // 14: Gray
        0xFFFFFFFF  // 15: White
    };

    auto writePixel = [&](int x, int y, uint8_t ci) {
        if (!buffer || x < 0 || x >= (int)width || y < 0 || y >= (int)height) return;
        uint32_t cv = palette[ci & 0x0F];
        uint32_t i = ((uint32_t)y * width + (uint32_t)x) * 4;
        buffer[i + 0] = (cv >>  0) & 0xFF; // B
        buffer[i + 1] = (cv >>  8) & 0xFF; // G
        buffer[i + 2] = (cv >> 16) & 0xFF; // R
        buffer[i + 3] = (cv >> 24) & 0xFF; // A
    };

    uint8_t bg_idx = registers[7] & 0x0F;
    uint32_t bg_val = palette[bg_idx];
    if (buffer) {
        for (uint32_t i = 0; i < width * height; ++i) {
            buffer[i * 4 + 0] = (bg_val >>  0) & 0xFF;
            buffer[i * 4 + 1] = (bg_val >>  8) & 0xFF;
            buffer[i * 4 + 2] = (bg_val >> 16) & 0xFF;
            buffer[i * 4 + 3] = (bg_val >> 24) & 0xFF;
        }
    }

    uint16_t name_table    = (registers[2] & 0x0F) << 10;
    uint16_t pattern_table = (registers[4] & 0x07) << 11;

    if (m1) {
        // TEXT mode (Screen 0): 40 columns × 24 rows, each char 6×8 px
        // 8 px border each side: x range [8, 247]
        uint8_t fg_idx = (registers[7] >> 4) & 0x0F;
        for (int row = 0; row < 24; row++) {
            for (int col = 0; col < 40; col++) {
                uint8_t ch = vram[(name_table + row * 40 + col) & 0x3FFF];
                for (int line = 0; line < 8; line++) {
                    uint8_t pat = vram[(pattern_table + ch * 8 + line) & 0x3FFF];
                    int sy = row * 8 + line;
                    for (int px = 0; px < 6; px++) {
                        bool on = (pat >> (7 - px)) & 1;
                        writePixel(8 + col * 6 + px, sy, on ? fg_idx : bg_idx);
                    }
                }
            }
        }
    } else if (m3) {
        // Graphic 2 (Screen 2): 32×24 tiles, per-row colour per tile
        for (int y = 0; y < (int)height; y++) {
            for (int x = 0; x < (int)width; x++) {
                int tile_x = x / 8, tile_y = y / 8;
                uint8_t tile = vram[(name_table + tile_y * 32 + tile_x) & 0x3FFF];
                int block = tile_y / 8;
                uint16_t pi = (((registers[4] & 0x04) << 11) | ((block & (registers[4] & 0x03)) << 11) | (tile << 3) | (y % 8)) & 0x3FFF;
                uint16_t ci = (((registers[3] & 0x80) << 6)  | ((block & (registers[3] & 0x7F)) << 11) | (tile << 3) | (y % 8)) & 0x3FFF;
                uint8_t pat   = vram[pi];
                uint8_t col   = vram[ci];
                uint8_t fg = (col >> 4) & 0x0F, bg = col & 0x0F;
                bool on = (pat >> (7 - (x % 8))) & 1;
                uint8_t idx = on ? fg : bg;
                if (idx == 0) idx = bg_idx;
                writePixel(x, y, idx);
            }
        }
    } else {
        // Graphic 1 (Screen 1): 32×24 tiles, 1 colour byte per 8 tiles
        uint16_t color_table = (registers[3] & 0xFF) << 6;
        for (int y = 0; y < (int)height; y++) {
            for (int x = 0; x < (int)width; x++) {
                int tile_x = x / 8, tile_y = y / 8;
                uint8_t tile = vram[(name_table + tile_y * 32 + tile_x) & 0x3FFF];
                uint8_t pat  = vram[(pattern_table + tile * 8 + (y % 8)) & 0x3FFF];
                uint8_t col  = vram[(color_table   + tile / 8)           & 0x3FFF];
                uint8_t fg = (col >> 4) & 0x0F, bg = col & 0x0F;
                bool on = (pat >> (7 - (x % 8))) & 1;
                uint8_t idx = on ? fg : bg;
                if (idx == 0) idx = bg_idx;
                writePixel(x, y, idx);
            }
        }
    }

    // Sprites — not present in TEXT mode
    if (!m1) {
        uint16_t sat = (registers[5] & 0x7F) << 7;
        uint16_t spt = (registers[6] & 0x07) << 11;
        int cnt[192] = {};
        for (int s = 0; s < 32; s++) {
            uint16_t base = sat + s * 4;
            if (base + 3 >= VRAM_SIZE) break;
            uint8_t yp = vram[base];
            if (yp == 0xD0) break;
            uint8_t xp  = vram[base + 1];
            uint8_t num = vram[base + 2];
            uint8_t col = vram[base + 3] & 0x0F;
            if (col == 0) continue;
            int ay = yp + 1;
            if (ay >= 240) ay -= 256;
            uint16_t pp = spt + num * 8;
            if (pp + 7 >= VRAM_SIZE) continue;
            for (int sy = 0; sy < 8; sy++) {
                int ry = ay + sy;
                if (ry < 0 || ry >= 192) continue;
                if (++cnt[ry] > 4) { status |= 0x40; continue; }
                uint8_t pat = vram[pp + sy];
                for (int sx = 0; sx < 8; sx++) {
                    if ((pat >> (7 - sx)) & 1)
                        writePixel(xp + sx, ry, col);
                }
            }
        }
    }
}
