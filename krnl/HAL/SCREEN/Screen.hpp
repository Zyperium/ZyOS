#pragma once
#include <stdint.h>

namespace HAL {
    class Screen {
    public:
        Screen(uint64_t scr_w, uint64_t scr_h, uint64_t scr_p, uint32_t *scr_addr);

        void put_pixel(int x, int y, uint32_t col);
        void fill(uint32_t col);

        void flip_buffer();

    protected:
        uint64_t screen_w, screen_h, screen_p;
        uint32_t *screen_addr;
        uint32_t *backbuffer;
    };
}