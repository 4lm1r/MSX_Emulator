# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
cd build
cmake ..
make
```

Or use the build script:

```bash
cd build && ./build.sh
```

Binaries are placed in `build/`: `msx_emulator` (main) and `msx_emulator_tests` (test suite).

## Tests

```bash
cd build && ./msx_emulator_tests
```

Run a specific test suite or test case using Google Test filters:

```bash
./msx_emulator_tests --gtest_filter="Z80ATest.*"
./msx_emulator_tests --gtest_filter="Z80ATest.LDA_n_Test"
```

## Run

The emulator requires `roms/MSX.ROM` (BIOS) to be present in the parent directory of the build location:

```bash
cd build && ./msx_emulator
```

## Architecture

This is a Z80A-based MSX computer emulator. The major hardware subsystems are each isolated in their own subdirectory under `src/` and use the singleton pattern (`getInstance()`).

### CPU (`src/cpu/`)
- `z80a.cpp` — Fetch/execute loop, registers, interrupt handling (IFF1/IFF2, EI_pending delay), and callback hooks for memory and I/O.
- `opcodesHandler.cpp` — Full Z80 instruction set including CB, ED, IX/IY (DD/FD), and DDCB/FDCB prefix dispatch. Maintains per-instruction cycle counts and flag helpers (`updateSZP`, `updateFlagsAdd`, etc.).

The CPU is decoupled from memory and peripherals entirely through four callbacks set at startup in `main.cpp`: `setMemoryReadCallback`, `setMemoryWriteCallback`, `setIOReadCallback`, `setIOWriteCallback`.

### Memory (`src/memory/`)
MSX memory uses a slot system: 4 primary slots (0–3), each optionally with 4 secondary subslots. Each subslot holds a `MemoryBlock` (either `RomBlock` or `RamBlock`). Active slot mapping is driven by PPI Port A via `mapPrimarySlots(uint8_t ppi_val)`. ROM blocks are write-protected. BIOS is loaded at startup with `loadROM("../roms/MSX.ROM", 0x0000)`.

### VDP (`src/vdp/`)
TMS9918A-compatible video processor. 16 KB VRAM. Ports 0x98 (data) and 0x99 (control/status). `update(cycles)` advances the internal cycle counter and fires a CPU interrupt at frame boundaries (59,736 cycles/frame ≈ 60 Hz). `render()` converts VRAM to an SDL pixel buffer. Implements read-ahead buffering on data port reads.

### PPI (`src/ppi/`)
8255-compatible parallel port. Port A (0xA8) drives slot selection via a callback into Memory. Port B (0xA9) reads the keyboard matrix. Port C (0xAA) handles cassette/speaker/keyboard row selection.

### PSG (`src/psg/`)
AY-3-8910 sound chip stub. Ports 0xA0 (address), 0xA1 (data write), 0xA2 (data read). 16 internal registers.

### Keyboard (`src/keyboard/`)
11-row MSX key matrix. `processEvent(SDL_Event&)` maps SDL keycodes to matrix positions. `readRow(row)` returns the 8-bit bitmask for PPI Port B reads.

### Peripheral stubs
`src/cartridge/`, `src/cassete/`, `src/floppy/`, `src/joystick/`, `src/paddle/`, `src/printer/`, `src/rs232c/` — all have test files but are not yet fully implemented.

### Main loop (`src/main.cpp`)
Initializes all singletons, wires CPU callbacks to memory/VDP/PPI/PSG, loads the BIOS ROM, opens an SDL2 512×384 window (256×192 × 2 zoom), then runs the per-frame loop: poll SDL events → execute ~59,736 CPU cycles → `vdp.update()` → `vdp.render()` → SDL present → 16 ms delay.

### Debug (`src/debug.h/cpp`)
Provides `debugLog(...)` for logging to a debug file. Used heavily during CPU and SP tracing.

## Key reference
`The MSX Red Book.pdf` in the repo root is the primary hardware reference for MSX memory map, I/O ports, VDP registers, and PPI behavior.
