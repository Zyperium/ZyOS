#pragma once
#include <stdint.h>
#include <stddef.h>

namespace TTY {
    void possess_host(int tty_id);

    enum class Callback {
        KEYBOARD_INPUT = 0,
        MOUSE_INPUT = 1
    };

    void hook_callback(int tty_id, Callback cb, void (*func_back)(uint64_t));

    enum class ScreenCTL {
        SET_COL,
        PUT_PIXEL,
        SWAP_BUFFER,
        DIRTY_RECT
    };

    namespace ScreenStructs {
        struct PIXEL_DATA {
            int x, y;
            uint32_t col;
        };

        struct DIRTY_DATA {
            int x, y;
            int width, height;
            bool redraw;
        };
    }

    void proc_screen_ctl(size_t tty_id, ScreenCTL control, uint64_t buf);
    uint32_t *get_tty_bbuffer();
}