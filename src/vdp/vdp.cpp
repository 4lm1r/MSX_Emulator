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
    registers[1] = 0xE0; // Enable display, 16x16 sprites, interrupts enabled (bit 5 set)
}

void VDP::update(int cycles) {
    cycle_counter += cycles;
    if (debug_log) {
        *debug_log << "VDP update: cycle_counter=" << cycle_counter << " after adding " << cycles << " cycles" << std::endl;
    }
    if (cycle_counter >= CYCLES_PER_FRAME) {
        cycle_counter -= CYCLES_PER_FRAME;
        if (cycle_counter < 0) cycle_counter = 0; // Prevent underflow
        // Set interrupt flag in status register (bit 7)
        status |= 0x80;
        if (debug_log) {
            *debug_log << "VDP: Frame complete, setting interrupt flag, status=0x" << std::hex << (int)status << std::dec << std::endl;
        }
        // If interrupts are enabled (register 1, bit 5) and CPU is set, trigger interrupt
        if ((registers[1] & 0x20) && cpu) {
            if (debug_log) {
                *debug_log << "VDP: Triggering interrupt at cycle " << cycle_counter << std::endl;
            }
            cpu->triggerInterrupt();
        } else {
            if (debug_log) {
                *debug_log << "VDP: Interrupt not triggered, interrupts disabled (reg1=0x" << std::hex << (int)registers[1] << std::dec << ")" << std::endl;
            }
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
    status = 0;
    is_second_byte = false;
    if (debug_log) {
        *debug_log << "readControlPort: status=0x" << std::hex << (int)temp << std::dec << std::endl;
    }
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
        // Always set VRAM address and mode, regardless of bit 7
        uint16_t addr = ((value & 0x3F) << 8) | temp_addr; // 14-bit address (bits 0-5 of second byte)
        vram_addr = addr & 0x3FFF; // Ensure within 16K VRAM
        write_mode = (value & 0x40) != 0; // Bit 6: 1 = write, 0 = read
        if (debug_log) {
            *debug_log << "writeControlPort: vram_addr=0x" << std::hex << vram_addr << " (write=" << (write_mode ? "true" : "false") << ")" << std::dec << std::endl;
            *debug_log << "Writing to VDP control port 0x99: 0x" << std::hex << (int)value << " at PC 0x" << cpu->PC << std::dec << std::endl;
        }

        // If bit 7 is set, also perform a register write
        if (value & 0x80) {
            uint8_t reg = value & 0x07; // Register number (0-7)
            registers[reg] = temp_addr; // Value from first byte
            if (debug_log) {
                *debug_log << "writeControlPort: reg=" << (int)reg << ", value=0x" << std::hex << (int)temp_addr << std::dec << std::endl;
            }
        }
    }
}

void VDP::render(uint8_t* buffer, uint32_t width, uint32_t height) {
    if (!(registers[1] & 0x40)) {
        if (debug_log) {
            *debug_log << "Display disabled" << std::endl;
        }
        return; // Display disabled
    }
    uint16_t name_table = (registers[2] & 0x0F) << 10;
    uint16_t pattern_table = (registers[4] & 0x07) << 11;
    uint16_t color_table = (registers[3] & 0xFF) << 6;
    uint16_t sprite_attribute_table = (registers[5] & 0x7F) << 7;
    uint16_t sprite_pattern_table = (registers[6] & 0x07) << 11;

    if (debug_log) {
        *debug_log << "render: name_table=0x" << std::hex << name_table
                  << ", pattern_table=0x" << pattern_table
                  << ", color_table=0x" << color_table
                  << ", sprite_attribute_table=0x" << sprite_attribute_table
                  << ", sprite_pattern_table=0x" << sprite_pattern_table << std::dec << std::endl;
    }

    // TMS9918A palette (BGRA format for SDL_PIXELFORMAT_BGRA8888)
    const uint32_t palette[16] = {
        0x000000FF, // 0: Transparent/Black
        0x000000FF, // 1: Black
        0x21C842FF, // 2: Medium Green
        0x5EDC78FF, // 3: Light Green
        0xED5554FF, // 4: Dark Blue
        0xFC767DFF, // 5: Light Blue
        0x4D52D4FF, // 6: Dark Red
        0xF5EB42FF, // 7: Cyan
        0x5455FCFF, // 8: Medium Red
        0x7879FFFF, // 9: Light Red
        0x54C1D4FF, // 10: Dark Yellow
        0x80CEE6FF, // 11: Light Yellow
        0x3BB021FF, // 12: Dark Green
        0xBA5BC9FF, // 13: Magenta
        0xCCCCCCFF, // 14: Gray
        0xFFFFFFFF  // 15: White
    };

    // Clear buffer with background color from register 7
    uint8_t bg_color_idx = registers[7] & 0x0F;
    uint32_t bg_color = palette[bg_color_idx];
    if (buffer) {
        std::memset(buffer, 0, width * height * 4); // Clear to black
    }

    // Render 32x24 tiles (256x192 pixels)
    for (uint32_t y = 0; y < height && y < 192; ++y) {
        for (uint32_t x = 0; x < width && x < 256; ++x) {
            uint8_t tile_x = x / 8;
            uint8_t tile_y = y / 8;
            uint16_t name_idx = name_table + (tile_y * 32 + tile_x);
            if (name_idx >= VRAM_SIZE) {
                if (debug_log) {
                    *debug_log << "Warning: name_idx out of bounds: 0x" << std::hex << name_idx << std::dec << std::endl;
                }
                continue;
            }
            uint8_t tile_idx = vram[name_idx];
            uint16_t pattern_idx = pattern_table + (tile_idx * 8);
            if (pattern_idx >= VRAM_SIZE) {
                if (debug_log) {
                    *debug_log << "Warning: pattern_idx out of bounds: 0x" << std::hex << pattern_idx << std::dec << std::endl;
                }
                continue;
            }
            uint8_t pattern = vram[pattern_idx + (y % 8)];
            uint16_t color_idx = color_table + tile_idx;
            if (color_idx >= VRAM_SIZE) {
                if (debug_log) {
                    *debug_log << "Warning: color_idx out of bounds: 0x" << std::hex << color_idx << std::dec << std::endl;
                }
                continue;
            }
            uint8_t color = vram[color_idx];
            uint8_t fg_color = (color >> 4) & 0x0F;
            uint8_t bg_color_idx_tile = color & 0x0F;
            bool pixel = (pattern >> (7 - (x % 8))) & 1;
            uint32_t color_val = pixel ? palette[fg_color] : palette[bg_color_idx_tile];
            uint32_t idx = (y * width + x) * 4;
            if (idx < width * height * 4 && buffer) {
                buffer[idx + 0] = (color_val >> 0) & 0xFF;  // B
                buffer[idx + 1] = (color_val >> 8) & 0xFF;  // G
                buffer[idx + 2] = (color_val >> 16) & 0xFF; // R
                buffer[idx + 3] = (color_val >> 24) & 0xFF; // A
            }

            if (x == 0 && y == 0) {
                if (debug_log) {
                    *debug_log << "First pixel: tile_idx=" << (int)tile_idx
                              << ", pattern=0x" << std::hex << (int)pattern
                              << ", color_idx=0x" << color_idx
                              << ", color=0x" << (int)color
                              << ", fg_color=0x" << (int)fg_color
                              << ", bg_color_idx=0x" << (int)bg_color_idx_tile
                              << ", pixel=" << pixel
                              << ", color_val=0x" << color_val << std::dec << std::endl;
                }
            }
        }
    }

    // Render sprites (8x8 for now, as set in register 1)
    if (debug_log) {
        *debug_log << "Starting sprite rendering..." << std::endl;
    }
    int sprite_count = 0;
    for (int sprite = 0; sprite < 32; sprite++) {
        uint16_t sprite_idx = sprite_attribute_table + (sprite * 4);
        if (debug_log) {
            *debug_log << "Checking sprite " << sprite << " at index 0x" << std::hex << sprite_idx << std::dec << std::endl;
        }
        if (sprite_idx + 3 >= VRAM_SIZE) {
            if (debug_log) {
                *debug_log << "Warning: sprite_idx out of bounds: 0x" << std::hex << sprite_idx << std::dec << std::endl;
            }
            break;
        }
        uint8_t y_pos = vram[sprite_idx];
        if (debug_log) {
            *debug_log << "Sprite " << sprite << " y_pos: 0x" << std::hex << (int)y_pos << std::dec << std::endl;
        }
        if (y_pos == 0xD0) {
            if (debug_log) {
                *debug_log << "Sprite list terminated at sprite " << sprite << std::endl;
            }
            break; // End of sprite list
        }
        uint8_t x_pos = vram[sprite_idx + 1];
        uint8_t pattern_num = vram[sprite_idx + 2];
        uint8_t color = vram[sprite_idx + 3] & 0x0F;

        if (debug_log) {
            *debug_log << "Processing sprite " << sprite << ": x=" << (int)x_pos << ", y=" << (int)y_pos
                      << ", pattern=" << (int)pattern_num << ", color=" << (int)color << std::endl;
        }

        if (color == 0) {
            if (debug_log) {
                *debug_log << "Sprite " << sprite << " skipped: transparent color" << std::endl;
            }
            continue; // Transparent color, skip
        }

        sprite_count++;
        if (sprite_count > 4) {
            if (debug_log) {
                *debug_log << "Sprite limit reached: " << sprite_count << " sprites" << std::endl;
            }
            status |= 0x40; // Set 5th sprite flag
            break; // Max 4 sprites per line (simplified)
        }

        uint16_t sprite_pattern_idx = sprite_pattern_table + (pattern_num * 8);
        if (sprite_pattern_idx >= VRAM_SIZE) {
            if (debug_log) {
                *debug_log << "Warning: sprite_pattern_idx out of bounds: 0x" << std::hex << sprite_pattern_idx << std::dec << std::endl;
            }
            continue;
        }

        // Adjust y_pos for TMS9918A: y=0 to 191 on-screen, 208+ terminates or off-screen
        int adjusted_y = y_pos;
        if (adjusted_y >= 208) {
            if (debug_log) {
                *debug_log << "Sprite " << sprite << " off-screen or terminated: y=" << (int)y_pos << std::endl;
            }
            continue;
        }

        if (debug_log) {
            *debug_log << "Sprite " << sprite << " adjusted y: " << adjusted_y << std::endl;
        }

        for (int sy = 0; sy < 8; sy++) {
            int screen_y = adjusted_y + sy;
            if (screen_y < 0 || screen_y >= 192) {
                if (debug_log) {
                    *debug_log << "Sprite " << sprite << " row " << sy << " out of bounds: screen_y=" << screen_y << std::endl;
                }
                continue;
            }
            uint8_t pattern = vram[sprite_pattern_idx + sy];
            if (debug_log) {
                *debug_log << "Sprite " << sprite << " row " << sy << " pattern: 0x" << std::hex << (int)pattern << std::dec << std::endl;
            }
            for (int sx = 0; sx < 8; sx++) {
                int screen_x = x_pos + sx;
                if (screen_x < 0 || screen_x >= 256) {
                    if (debug_log) {
                        *debug_log << "Sprite " << sprite << " col " << sx << " out of bounds: screen_x=" << screen_x << std::endl;
                    }
                    continue;
                }
                bool pixel = (pattern >> (7 - sx)) & 1;
                if (debug_log) {
                    *debug_log << "Sprite " << sprite << " pixel (" << sx << "," << sy << "): " << pixel << std::endl;
                }
                if (!pixel) continue; // Transparent pixel
                uint32_t idx = (screen_y * width + screen_x) * 4;
                if (idx < width * height * 4 && buffer) {
                    uint32_t color_val = palette[color];
                    buffer[idx + 0] = (color_val >> 0) & 0xFF;  // B
                    buffer[idx + 1] = (color_val >> 8) & 0xFF;  // G
                    buffer[idx + 2] = (color_val >> 16) & 0xFF; // R
                    buffer[idx + 3] = (color_val >> 24) & 0xFF; // A
                    if (debug_log) {
                        *debug_log << "Drawing sprite pixel at (" << screen_x << "," << screen_y << ") with color 0x"
                                  << std::hex << color_val << std::dec << std::endl;
                    }
                }
            }
        }
    }
}
