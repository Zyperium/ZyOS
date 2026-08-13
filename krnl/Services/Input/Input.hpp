#pragma once
#include <stdint.h>

namespace Input {
    void add_kb(char nc);
    void reg_kb_cb(void (*callback)(char c));

    // later!
    struct MousePos {
        uint32_t x;
        uint32_t y;
    } __attribute__((packed));

    MousePos get_mouse();
}