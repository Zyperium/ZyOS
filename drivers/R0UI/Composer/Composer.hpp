#pragma once
#include <stdint.h>
#include <stddef.h>

namespace R0UI::Composer {
    void worker1(uint32_t *tty_bbuf);

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

    enum class CMPSTR_STATE : size_t {
        NONE,
        INIT,
        RUNNING,
        HANDLE_KB,
        HANDLE_MOUSE,
        SHUTDOWN,
        SIZE
    };
}