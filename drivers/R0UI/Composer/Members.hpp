#pragma once
#include <stdint.h>
#include <stddef.h>

#include <SERVICES.hpp>
#include <lib/locks.hpp>

namespace R0UI {
    struct P1D {
        int x, y;
    };

    struct P2D : P1D {
        int width, height;
    };

    struct ICO : P2D {
        uint32_t *buf;
    };

    struct WinControl {
        int32_t x, y;
        uint32_t width, height;
        uint32_t z_index;
        uint32_t flags;
        uint32_t *usr_pix_buf;
        uint8_t reserved[4060];
        bool read;
    };

    static_assert(sizeof(WinControl) == 4096, "WinControl MUST be 4096 bytes");

    class Window {
    public:
        Window(P2D def);
        ~Window();

        WinControl *winref;
        P2D factposn;
        uint64_t owner; // PID owner

        void move(int nx, int ny);
        void resize(int nwidth,  int nheight);
        void paint(uint32_t *screen);
        /**
            This one is a little weird, so quick explanar:
            once you setup a window, you use this function
            (which returns a VIRTUAL address to the memory)
            to map the buffer to whatever task is passed here.
        */
        uint32_t *map_to(Scheduler::Task *pass_to);
    
        static constexpr size_t DEFAULT_WINDOW_SIZE_W = 500;
        static constexpr size_t DEFAULT_WINDOW_SIZE_H = 350;

        static constexpr size_t WINDOWED_PADDING_AMOUNT = 10;
    private:
        uint32_t *buffer;
        char *title;
        ICO icon;
    };

    struct winpair {
        Window *ref;
        volatile winpair*next;
        volatile winpair *prev;
    };

    extern lib::Spinlock linklock;
    extern volatile winpair *linked_io;
}