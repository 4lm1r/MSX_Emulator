#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <cstdint>
#include <SDL2/SDL.h>

class Keyboard {
public:
    static Keyboard& getInstance();
    void reset();
    
    void processEvent(const SDL_Event& event);
    uint8_t readRow(uint8_t row) const;

private:
    Keyboard();
    uint8_t matrix[11]; // MSX keyboard matrix (11 rows)
    
    void updateMatrix(SDL_Keycode key, bool pressed);
};

#endif
