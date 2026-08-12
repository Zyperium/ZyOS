#pragma once
#include <stdint.h>

namespace Input {
    void add_kb(char nc);
    char pop_kb();

    // later!
    struct MousePos {
        uint32_t x;
        uint32_t y;
    } __attribute__((packed));

    MousePos get_mouse();
}