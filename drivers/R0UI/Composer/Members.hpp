#pragma once

#include <stdint.h>
#include <stddef.h>

#include <SERVICES.hpp>
#include <lib/locks.hpp>
#include "r0ui_protocol.hpp"

namespace R0UI {
    class Window {
    public:
        Window(Rect def);
        ~Window();

        void move(int32_t nx, int32_t ny);
        void resize(uint32_t nwidth, uint32_t nheight);
        void paint(uint32_t *screen);
        void readref(Scheduler::Task *ref);
        
        uint32_t *map_to(Scheduler::Task *pass_to);
        bool push_event(const Event &ev);

        static constexpr size_t DEFAULT_WINDOW_SIZE_W = 500;
        static constexpr size_t DEFAULT_WINDOW_SIZE_H = 350;
        static constexpr size_t WINDOWED_PADDING_AMOUNT = 10;

        WinControl *winref{nullptr};
        uint64_t usr_pix{0};
        Rect factposn{};
        uint64_t owner{0}; 

    private:
        uint32_t *buffer{nullptr};
        char title[64]{0};

        void realloc_pixel_buffer(Scheduler::Task *ref, uint32_t old_w, uint32_t old_h);
    };

    struct winpair {
        Window *ref;
        winpair *next;
        winpair *prev;
    };

    extern lib::Spinlock linklock;
    extern winpair *linked_io;
}