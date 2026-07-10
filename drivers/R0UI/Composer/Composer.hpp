#pragma once
#include <stdint.h>

namespace R0UI::Composer {
    void worker1();
    void worker2(uint32_t *tty_bbuf);

    void add_damage(int x, int y, int w, int h);
    void force_redraw();
    void handle_input(uint64_t k);

    struct IUPDATE {
        char key;
        bool pressed;

        int x, y;
        bool m1, m2;
        IUPDATE *next;
    };
}