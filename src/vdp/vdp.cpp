#include "vdp.h"
#include "z80a.h"
#include <cstring>

VDP& VDP::getInstance() {
    static VDP instance;
    return instance;
}

VDP::VDP() : cpu(nullptr), debug_log(nullptr), status(0), vram_addr(0), read_buffer(0), 
            is_second_byte(false), write_mode(false), cycle_counter(0) {
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
    write_mode = false; // Ensure default state
    // Set default mode: Graphic Mode 2
    registers[0] = 0x00; // External video off, 16K VRAM
    registers[1] = 0x80; // Enable display, 16x16 sprites, interrupts DISABLED initially (bit 5 cleared)
}

void VDP::update(int cycles) {
    cycle_counter += cycles;
    if (cycle_counter >= CYCLES_PER_FRAME) {
        cycle_counter -= CYCLES_PER_FRAME;
        if (cycle_counter < 0) cycle_counter = 0; // Prevent underflow
        // Set interrupt flag in status register (bit 7)
        status |= 0x80;
        // If interrupts are enabled (register 1, bit 5) and CPU is set, trigger interrupt
        if ((registers[1] & 0x20) && cpu) {
            cpu->triggerInterrupt();
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
    // Clear the interrupt flag when status is read
    status &= ~0x80;
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
        if (debug_log) {
            *debug_log << "Display disabled" << std::endl;
        }
        if (buffer) {
            std::memset(buffer, 0, width * height * 4); // Show black if disabled
        }
        return; // Display disabled
    }

    // Determine Mode
    bool m1 = (registers[1] >> 4) & 1;
    bool m2 = (registers[1] >> 3) & 1;
    bool m3 = (registers[0] >> 1) & 1;

    uint16_t name_table = (registers[2] & 0x0F) << 10;
    uint16_t pattern_table = (registers[4] & 0x07) << 11;
    uint16_t color_table = 0;
    
    if (m3) {
        // Graphic 2
        color_table = (registers[3] & 0x80) << 6;
    } else {
        // Graphic 1
        color_table = (registers[3] & 0xFF) << 6;
    }

    uint16_t sprite_attribute_table = (registers[5] & 0x7F) << 7;
    uint16_t sprite_pattern_table = (registers[6] & 0x07) << 11;

    // TMS9918A palette (Values in 0xAARRGGBB format for my extraction logic)
    const uint32_t palette[16] = {
        0x00000000, // 0: Transparent
        0xFF000000, // 1: Black
        0xFF21C842, // 2: Medium Green
        0xFF5EDC78, // 3: Light Green
        0xFFED5554, // 4: Dark Blue
        0xFFFC767D, // 5: Light Blue
        0xFF4D52D4, // 6: Dark Red
        0xFFF5EB42, // 7: Cyan
        0xFF5455FC, // 8: Medium Red
        0xFF7879FF, // 9: Light Red
        0xFF54C1D4, // 10: Dark Yellow
        0xFF80CEE6, // 11: Light Yellow
        0xFF3BB021, // 12: Dark Green
        0xFFBA5BC9, // 13: Magenta
        0xFFCCCCCC, // 14: Gray
        0xFFFFFFFF  // 15: White
    };

    // Clear buffer with background color from register 7
    uint8_t bg_color_idx = registers[7] & 0x0F;
    uint32_t bg_color_val = palette[bg_color_idx];
    if (buffer) {
        for (uint32_t i = 0; i < width * height; ++i) {
            buffer[i * 4 + 0] = (bg_color_val >> 0) & 0xFF;  // B
            buffer[i * 4 + 1] = (bg_color_val >> 8) & 0xFF;  // G
            buffer[i * 4 + 2] = (bg_color_val >> 16) & 0xFF; // R
            buffer[i * 4 + 3] = (bg_color_val >> 24) & 0xFF; // A
        }
    }

    // Render 32x24 tiles (256x192 pixels)
    for (uint32_t y = 0; y < 192; ++y) {
        for (uint32_t x = 0; x < 256; ++x) {
            uint8_t tile_x = x / 8;
            uint8_t tile_y = y / 8;
            uint16_t name_idx = name_table + (tile_y * 32 + tile_x);
            if (name_idx >= VRAM_SIZE) continue;
            
            uint8_t tile_idx = vram[name_idx];
            uint16_t pattern_idx, color_idx;
            
            if (m3) {
                // Graphic 2 logic
                int block = (tile_y / 8); // 0, 1, or 2
                uint16_t pattern_base = (registers[4] & 0x04) << 11;
                uint16_t color_base = (registers[3] & 0x80) << 6;
                
                pattern_idx = pattern_base + ((block & (registers[4] & 0x03)) << 11) + (tile_idx << 3) + (y % 8);
                color_idx = color_base + ((block & (registers[3] & 0x7F)) << 11) + (tile_idx << 3) + (y % 8);
            } else {
                // Graphic 1 logic
                pattern_idx = pattern_table + (tile_idx * 8) + (y % 8);
                color_idx = color_table + (tile_idx / 8);
            }

            if (pattern_idx >= VRAM_SIZE || color_idx >= VRAM_SIZE) continue;

            uint8_t pattern = vram[pattern_idx];
            uint8_t color_byte = vram[color_idx];
            uint8_t fg_color = (color_byte >> 4) & 0x0F;
            uint8_t bg_color = color_byte & 0x0F;
            
            bool pixel = (pattern >> (7 - (x % 8))) & 1;
            uint8_t final_color_idx = pixel ? fg_color : bg_color;
            if (final_color_idx == 0) final_color_idx = bg_color_idx; // Transparent -> BG color
            
            uint32_t color_val = palette[final_color_idx];
            uint32_t idx = (y * width + x) * 4;
            if (idx < width * height * 4 && buffer) {
                buffer[idx + 0] = (color_val >> 0) & 0xFF;  // B
                buffer[idx + 1] = (color_val >> 8) & 0xFF;  // G
                buffer[idx + 2] = (color_val >> 16) & 0xFF; // R
                buffer[idx + 3] = (color_val >> 24) & 0xFF; // A
            }
        }
    }

    // Render sprites
    int sprites_on_line[192] = {0};
    for (int sprite = 0; sprite < 32; sprite++) {
        uint16_t sprite_idx = sprite_attribute_table + (sprite * 4);
        if (sprite_idx + 3 >= VRAM_SIZE) break;

        uint8_t y_pos = vram[sprite_idx];
        if (y_pos == 0xD0) break; // End of sprite list

        uint8_t x_pos = vram[sprite_idx + 1];
        uint8_t pattern_num = vram[sprite_idx + 2];
        uint8_t color = vram[sprite_idx + 3] & 0x0F;

        if (color == 0) continue; // Transparent

        int adjusted_y = y_pos + 1; // y_pos is coordinate-1
        if (adjusted_y >= 240) adjusted_y -= 256; // Wraparound for sprites starting above top

        uint16_t sprite_pattern_idx = sprite_pattern_table + (pattern_num * 8);
        if (sprite_pattern_idx + 7 >= VRAM_SIZE) continue;

        for (int sy = 0; sy < 8; sy++) {
            int screen_y = adjusted_y + sy;
            if (screen_y < 0 || screen_y >= 192) continue;
            
            if (++sprites_on_line[screen_y] > 4) {
                status |= 0x40; // 5th sprite flag
                // continue; // TMS9918A drops 5th sprite on line
            }

            uint8_t pattern = vram[sprite_pattern_idx + sy];
            for (int sx = 0; sx < 8; sx++) {
                int screen_x = x_pos + sx;
                if (screen_x < 0 || screen_x >= 256) continue;
                
                if ((pattern >> (7 - sx)) & 1) {
                    uint32_t idx = (screen_y * width + screen_x) * 4;
                    if (idx < width * height * 4 && buffer) {
                        uint32_t color_val = palette[color];
                        buffer[idx + 0] = (color_val >> 0) & 0xFF;  // B
                        buffer[idx + 1] = (color_val >> 8) & 0xFF;  // G
                        buffer[idx + 2] = (color_val >> 16) & 0xFF; // R
                        buffer[idx + 3] = (color_val >> 24) & 0xFF; // A
                    }
                }
            }
        }
    }
}
