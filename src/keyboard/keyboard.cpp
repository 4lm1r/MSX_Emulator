#include "keyboard.h"
#include <cstring>

Keyboard& Keyboard::getInstance() {
    static Keyboard instance;
    return instance;
}

Keyboard::Keyboard() {
    reset();
}

void Keyboard::reset() {
    std::memset(matrix, 0xFF, sizeof(matrix)); // 0xFF means no keys pressed (bits are 0 when pressed)
}

uint8_t Keyboard::readRow(uint8_t row) const {
    if (row < 11) return matrix[row];
    return 0xFF;
}

void Keyboard::processEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        updateMatrix(event.key.keysym.sym, event.type == SDL_KEYDOWN);
    }
}

void Keyboard::updateMatrix(SDL_Keycode key, bool pressed) {
    auto set = [&](int row, int bit) {
        if (row >= 11) return;
        if (pressed) matrix[row] &= ~(1 << bit);
        else         matrix[row] |=  (1 << bit);
    };

    // MSX keyboard matrix layout:
    //       Bit7  Bit6  Bit5  Bit4  Bit3  Bit2  Bit1  Bit0
    // Row0:  7     6     5     4     3     2     1     0
    // Row1:  ;     ]     [     \     =     -     9     8
    // Row2:  B     A    DEAD  CAPS  GRPH  CTRL  CODE  SHFT
    // Row3:  J     I     H     G     F     E     D     C
    // Row4:  R     Q     P     O     N     M     L     K
    // Row5:  Z     Y     X     W     V     U     T     S
    // Row6:  F3    F2    F1   KANA   /     .     ,     '
    // Row7:  RET  SEL   BS   STOP  TAB   ESC    F5    F4
    // Row8: RIGHT DOWN  UP   LEFT  DEL   INS   HOME  SPC
    // Row9:  4p    3p   2p    1p    0p    /p    +p    *p
    // Row10: .p    ,p   -p    9p    8p    7p    6p    5p

    switch (key) {
        // Row 0 — digit keys 0-7
        case SDLK_0: set(0, 0); break;
        case SDLK_1: set(0, 1); break;
        case SDLK_2: set(0, 2); break;
        case SDLK_3: set(0, 3); break;
        case SDLK_4: set(0, 4); break;
        case SDLK_5: set(0, 5); break;
        case SDLK_6: set(0, 6); break;
        case SDLK_7: set(0, 7); break;

        // Row 1 — digits 8-9, punctuation
        case SDLK_8:            set(1, 0); break;
        case SDLK_9:            set(1, 1); break;
        case SDLK_MINUS:        set(1, 2); break;
        case SDLK_EQUALS:       set(1, 3); break;
        case SDLK_BACKSLASH:    set(1, 4); break;
        case SDLK_LEFTBRACKET:  set(1, 5); break;
        case SDLK_RIGHTBRACKET: set(1, 6); break;
        case SDLK_SEMICOLON:    set(1, 7); break;

        // Row 2 — modifiers + B, A
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:   set(2, 0); break;
        case SDLK_LCTRL:
        case SDLK_RCTRL:    set(2, 2); break;
        case SDLK_LALT:
        case SDLK_RALT:     set(2, 3); break; // GRAPH
        case SDLK_CAPSLOCK: set(2, 4); break;
        case SDLK_a:        set(2, 6); break;
        case SDLK_b:        set(2, 7); break;

        // Row 3 — C-J
        case SDLK_c: set(3, 0); break;
        case SDLK_d: set(3, 1); break;
        case SDLK_e: set(3, 2); break;
        case SDLK_f: set(3, 3); break;
        case SDLK_g: set(3, 4); break;
        case SDLK_h: set(3, 5); break;
        case SDLK_i: set(3, 6); break;
        case SDLK_j: set(3, 7); break;

        // Row 4 — K-R
        case SDLK_k: set(4, 0); break;
        case SDLK_l: set(4, 1); break;
        case SDLK_m: set(4, 2); break;
        case SDLK_n: set(4, 3); break;
        case SDLK_o: set(4, 4); break;
        case SDLK_p: set(4, 5); break;
        case SDLK_q: set(4, 6); break;
        case SDLK_r: set(4, 7); break;

        // Row 5 — S-Z
        case SDLK_s: set(5, 0); break;
        case SDLK_t: set(5, 1); break;
        case SDLK_u: set(5, 2); break;
        case SDLK_v: set(5, 3); break;
        case SDLK_w: set(5, 4); break;
        case SDLK_x: set(5, 5); break;
        case SDLK_y: set(5, 6); break;
        case SDLK_z: set(5, 7); break;

        // Row 6 — punctuation, F1-F3
        case SDLK_QUOTE:  set(6, 0); break;
        case SDLK_COMMA:  set(6, 1); break;
        case SDLK_PERIOD: set(6, 2); break;
        case SDLK_SLASH:  set(6, 3); break;
        case SDLK_F1:     set(6, 5); break;
        case SDLK_F2:     set(6, 6); break;
        case SDLK_F3:     set(6, 7); break;

        // Row 7 — control keys
        case SDLK_F4:        set(7, 0); break;
        case SDLK_F5:        set(7, 1); break;
        case SDLK_ESCAPE:    set(7, 2); break;
        case SDLK_TAB:       set(7, 3); break;
        case SDLK_PAUSE:     set(7, 4); break; // STOP
        case SDLK_BACKSPACE: set(7, 5); break;
        case SDLK_END:       set(7, 6); break; // SELECT
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  set(7, 7); break;

        // Row 8 — navigation
        case SDLK_SPACE:  set(8, 0); break;
        case SDLK_HOME:   set(8, 1); break;
        case SDLK_INSERT: set(8, 2); break;
        case SDLK_DELETE: set(8, 3); break;
        case SDLK_LEFT:   set(8, 4); break;
        case SDLK_UP:     set(8, 5); break;
        case SDLK_DOWN:   set(8, 6); break;
        case SDLK_RIGHT:  set(8, 7); break;

        // Row 9 — numpad * + / 0-4
        case SDLK_KP_MULTIPLY: set(9, 0); break;
        case SDLK_KP_PLUS:     set(9, 1); break;
        case SDLK_KP_DIVIDE:   set(9, 2); break;
        case SDLK_KP_0:        set(9, 3); break;
        case SDLK_KP_1:        set(9, 4); break;
        case SDLK_KP_2:        set(9, 5); break;
        case SDLK_KP_3:        set(9, 6); break;
        case SDLK_KP_4:        set(9, 7); break;

        // Row 10 — numpad 5-9, -, ., ,
        case SDLK_KP_5:      set(10, 0); break;
        case SDLK_KP_6:      set(10, 1); break;
        case SDLK_KP_7:      set(10, 2); break;
        case SDLK_KP_8:      set(10, 3); break;
        case SDLK_KP_9:      set(10, 4); break;
        case SDLK_KP_MINUS:  set(10, 5); break;
        case SDLK_KP_PERIOD: set(10, 7); break;

        default: break;
    }
}
