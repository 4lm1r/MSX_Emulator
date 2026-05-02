#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <deque>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <SDL2/SDL.h>
#include "cpu/z80a.h"
#include "vdp/vdp.h"
#include "memory/memory.h"
#include "ppi/ppi.h"
#include "keyboard/keyboard.h"
#include "psg/psg.h"
#include "debug.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

static constexpr int SCALE  = 3;
static constexpr int EMU_W  = 256;
static constexpr int EMU_H  = 192;
static constexpr int MENU_H = 19;
static constexpr int WIN_W  = EMU_W * SCALE;
static constexpr int WIN_H  = EMU_H * SCALE + MENU_H;
static constexpr int MAX_RECENT = 5;

static const std::string BIOS_PATH = "../roms/MSX.ROM";

// --- Recent-files helpers ---

static std::string recentFilePath() {
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/msxemulator/recent.txt";
}

static std::deque<std::string> loadRecentFiles() {
    std::deque<std::string> files;
    std::ifstream f(recentFilePath());
    std::string line;
    while (std::getline(f, line))
        if (!line.empty() && (int)files.size() < MAX_RECENT)
            files.push_back(line);
    return files;
}

static void saveRecentFiles(const std::deque<std::string>& files) {
    auto path = recentFilePath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path);
    for (const auto& p : files) f << p << "\n";
}

static void addRecentFile(std::deque<std::string>& files, const std::string& path) {
    auto it = std::find(files.begin(), files.end(), path);
    if (it != files.end()) files.erase(it);
    files.push_front(path);
    if ((int)files.size() > MAX_RECENT) files.pop_back();
    saveRecentFiles(files);
}

// --- Native file picker via zenity ---

static std::string showFileDialog() {
    FILE* pipe = popen(
        "zenity --file-selection"
        " --title='Open MSX Cartridge'"
        " --file-filter='MSX ROM files (*.rom *.mx1 *.mx2) | *.rom *.ROM *.mx1 *.MX1 *.mx2 *.MX2'"
        " --file-filter='All files | *'"
        " 2>/dev/null",
        "r");
    if (!pipe) return "";
    char buf[4096];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// --- Main ---

int main(int argc, char* argv[]) {
    Z80A cpu;
    Memory&   memory   = Memory::getInstance();
    VDP&      vdp      = VDP::getInstance();
    PPI&      ppi      = PPI::getInstance();
    Keyboard& keyboard = Keyboard::getInstance();
    PSG&      psg      = PSG::getInstance();

    auto resetAll = [&]() {
        cpu.reset();
        memory.reset();
        vdp.reset();
        ppi.reset();
        keyboard.reset();
        psg.reset();
        vdp.setCPU(&cpu);
        vdp.setDebugLogStream(debug_log);
    };

    auto wireCPU = [&]() {
        cpu.setMemoryReadCallback([&](uint16_t addr) { return memory.read(addr); });
        cpu.setMemoryWriteCallback([&](uint16_t addr, uint8_t val) { memory.write(addr, val); });
        cpu.setIOReadCallback([&](uint8_t port) -> uint8_t {
            if (port == 0x98) return vdp.readDataPort();
            if (port == 0x99) return vdp.readControlPort();
            if (port == 0xA2) return psg.readData();
            if ((port & 0xFC) == 0xA8) return ppi.read(port & 0x03);
            return 0xFF;
        });
        cpu.setIOWriteCallback([&](uint8_t port, uint8_t val) {
            if      (port == 0x98)           vdp.writeDataPort(val);
            else if (port == 0x99)           vdp.writeControlPort(val);
            else if (port == 0xA0)           psg.writeAddress(val);
            else if (port == 0xA1)           psg.writeData(val);
            else if ((port & 0xFC) == 0xA8)  ppi.write(port & 0x03, val);
        });
    };

    resetAll();
    wireCPU();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window*   window   = SDL_CreateWindow("MSX Emulator",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture*  texture  = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888,
                                 SDL_TEXTUREACCESS_STREAMING, EMU_W, EMU_H);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    psg.initAudio();

    if (!memory.loadROM(BIOS_PATH, 0x0000)) {
        std::cerr << "CRITICAL: Failed to load BIOS from " << BIOS_PATH << std::endl;
        return 1;
    }

    auto recentFiles     = loadRecentFiles();
    std::string currentCartridge;
    bool        pendingOpenDialog   = false;
    std::string pendingCartridgePath;
    bool        running             = true;
    SDL_Event   event;
    uint8_t     screen_buffer[EMU_W * EMU_H * 4];

    std::ofstream watchdog_file("watchdog.log", std::ios::trunc);
    uint64_t      total_instr = 0;

    std::cout << "Starting emulation loop..." << std::endl;

    while (running) {
        // --- Event polling ---
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (!io.WantCaptureKeyboard) {
                // Ctrl+O: open file dialog
                if (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_o &&
                    (event.key.keysym.mod & KMOD_CTRL)) {
                    pendingOpenDialog = true;
                } else {
                    keyboard.processEvent(event);
                }
            }
        }

        // --- Deferred file-open (runs after ImGui rendered the menu closed) ---
        if (pendingOpenDialog) {
            pendingOpenDialog = false;
            std::string path = showFileDialog();
            if (!path.empty()) pendingCartridgePath = path;
        }
        if (!pendingCartridgePath.empty()) {
            std::string path = std::move(pendingCartridgePath);
            pendingCartridgePath.clear();
            resetAll();
            wireCPU();
            if (memory.loadROM(BIOS_PATH, 0x0000) && memory.loadCartridge(path)) {
                currentCartridge = path;
                addRecentFile(recentFiles, path);
                total_instr = 0;
            } else {
                std::cerr << "Failed to load cartridge: " << path << std::endl;
                memory.loadROM(BIOS_PATH, 0x0000); // recover BIOS-only state
            }
        }

        // --- CPU execution (one frame worth of cycles) ---
        int frame_cycles = 0;
        while (frame_cycles < 59736) {
            if (total_instr % 10000 == 0) {
                watchdog_file << "WATCHDOG: PC=0x" << std::hex << cpu.PC
                              << " SP=0x"  << cpu.SP
                              << " A=0x"   << (int)cpu.A
                              << " IFF1="  << (int)cpu.IFF1
                              << std::dec  << " n=" << total_instr << "\n";
                watchdog_file.flush();
            }
            ++total_instr;
            int cycles = cpu.execute();
            vdp.update(cycles);
            psg.update(cycles);
            frame_cycles += cycles;
        }

        // --- Render emulator screen below the menu bar ---
        vdp.render(screen_buffer, EMU_W, EMU_H);
        SDL_UpdateTexture(texture, nullptr, screen_buffer, EMU_W * 4);
        SDL_RenderClear(renderer);
        SDL_Rect dest = {0, MENU_H, WIN_W, WIN_H - MENU_H};
        SDL_RenderCopy(renderer, texture, nullptr, &dest);

        // --- ImGui menu bar ---
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Cartridge...", "Ctrl+O"))
                    pendingOpenDialog = true;

                if (!recentFiles.empty() && ImGui::BeginMenu("Recent Files")) {
                    for (const auto& f : recentFiles) {
                        auto label = std::filesystem::path(f).filename().string();
                        if (ImGui::MenuItem(label.c_str()))
                            pendingCartridgePath = f;
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", f.c_str());
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4"))
                    running = false;
                ImGui::EndMenu();
            }

            // Show loaded cartridge name in the bar
            if (!currentCartridge.empty()) {
                auto name = "  \xe2\x80\x94  " +
                            std::filesystem::path(currentCartridge).filename().string();
                ImGui::TextDisabled("%s", name.c_str());
            }

            ImGui::EndMainMenuBar();
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // --- Cleanup ---
    psg.closeAudio();
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
