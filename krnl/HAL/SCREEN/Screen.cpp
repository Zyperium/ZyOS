#include <HAL/SCREEN/Screen.hpp>

namespace HAL {
    Screen::Screen(uint64_t scr_w, uint64_t scr_h, uint64_t scr_p, uint32_t *scr_addr) : screen_w(scr_w), screen_h(scr_h),
        screen_p(scr_p), screen_addr(scr_addr) {
            
    }
}