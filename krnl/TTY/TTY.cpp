#include "Library/debug.hpp"
#include <TTY/TTY.hpp>

#include <HAL/SCREEN/Screen.hpp>
#include <HAL/SCREEN/Font.hpp>

using namespace HAL::SCREEN;

namespace TTY {
    ConHost *conhosts[MAX_VIRTUAL_CONSHOSTS];
    size_t active_host = 0;
    size_t total_hosts = 0;
    size_t off_x = 0;
    size_t off_y = 0;

    ConHost::ConHost() {
        conhosts[total_hosts] = this;
        cohost_id = total_hosts++;
        cur_input = new char[MAX_TERMINAL_TEXT];
        return;
    }

    void ConHost::send_input(char c) {
        if (c == '\b') {
            if (off_x == 0) return;
            --off_x;
            cur_input[off_x] = 0;
            draw_char(' ', off_x * Font::WIDTH, off_y * Font::HEIGHT, COL::BLACK);
            flip_buffer();
            return;
        }

        if (off_x >= MAX_TERMINAL_TEXT) {
            Debug::krnl_print("TTY", Debug::LOG_INFO, "Max text length reached!");
            return;
        }

        cur_input[off_x] = c;
        if (c != ' ')
            draw_char(c, off_x * Font::WIDTH, off_y * Font::HEIGHT, COL::WHITE);
        ++off_x;
        flip_buffer();
    }

    void ConHost::reset_view() {
        // clear the screen
    }
    
    void ConHost::worker() {
        Debug::krnl_print("TTY", Debug::LOG_INFO, "ConHost %i worker begin", cohost_id);
        fill_screen(COL::BLACK);
        for (;;) {
            if (!contask) continue;
            
        }
    }
}