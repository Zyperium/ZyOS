#pragma once
#include <stdint.h>

namespace Input {
    void add_kb(char nc);
    void reg_kb_cb(void (*callback)(char c));
    
    // now
    struct MousePos {
        int8_t delta_x;
        int8_t delta_y;
        uint8_t buttons;
        int8_t scroll_wheel;
    } __attribute__((packed));

    void add_mouse(const MousePos ref);
    void reg_ms_cb(void (*xmscallback)(const MousePos ref));
}