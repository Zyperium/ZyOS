#include "Scheduler/Scheduler.hpp"
#include <Library/debug.hpp>
#include <Library/string.h>
#include <TTY/TTY.hpp>

#include <HAL/SCREEN/Screen.hpp>
#include <HAL/SCREEN/Font.hpp>
#include <HAL/DISK/Disk.hpp>

using namespace HAL::SCREEN;

namespace TTY {
    ConHost *conhosts[MAX_VIRTUAL_CONSHOSTS];
    size_t active_host = 0;
    size_t total_hosts = 0;
    size_t off_x = 0;
    size_t off_y = 0;
    size_t raw_x = 0;
    size_t raw_y = 0;

    char drv_str[] = {'[', '?', ':', '/', '>', ' ', 0};

    int kernel_atoi(const char* str) {
        if (!str || *str == '\0') {
            return -1;
        }

        long long result = 0;

        for (int i = 0; str[i] != '\0'; ++i) {
            char c = str[i];

            if (c < '0' || c > '9') {
                return -1;
            }

            result = (result * 10) + (c - '0');

            if (result > 2147483647) {
                return -1;
            }
        }

        return static_cast<int>(result);
    }


    /**
        I know this is a horrible way of doing this.
        I am simply trying to run some bug/early testing. It'll be replaced with a more streamlined
        dictionary style command system
    */
    void ProcessCommand(const char *str) {
        ConHost *r_host = conhosts[active_host];
        if (strcmp(str, "help")) {
            r_host->draw_string("== HELP ==\n");
            r_host->draw_string("echo : Echo arguments back\n");
            r_host->draw_string("pid  : Arg1: int (PID); Fetch information on some PID given in Arg1\n");
            r_host->draw_string("help : Show this dialog\n\n");
            if (drv_str[1] == '?') {
                r_host->draw_string("Warning: ", COL::YELLOW);
                r_host->draw_string("you are currently on the "); r_host->draw_string("unknown disk ", COL::RED);
                r_host->draw_string("[?/:>. Default drive is typically [A:/>. Type 'A:/' to swap to it.\n");
                r_host->draw_string("(This may render some commands useless)\n");
            }
        }
        else if (str[1] == ':' && str[2] == '/') {
            if (HAL::DISK::IsValidDisk(str[0])) {
                drv_str[1] = str[0];
            }
            else {
                r_host->draw_string("Error: ", COL::RED);
                r_host->draw_string("drive is unavailable! [It may be on a different letter]\n");
            }
        }
        else if (str[0] == 'p' && str[1] == 'i' && str[2] == 'd' && str[3] == ' ' && str[4] != 0) {
            int kint = kernel_atoi(&str[4]);
            Scheduler::Task *val_ = Scheduler::GetTaskByPID(kint);
            char mini_buf[64]{0};
            if (!val_) {
                Debug::snprintf(mini_buf, 64, "PID %i is non-existant.\n", kint);
            }
            else {
                Debug::snprintf(mini_buf, 64, "PID %i is named %s and is operating on core %i\n", kint, val_->task_name.c_str(), val_->current_core);
            }

            r_host->draw_string(mini_buf);
        }
        else if (str[0] == 'e' && str[1] == 'c' && str[2] == 'h' && str[3] == 'o' && str[4] == ' ') {
            r_host->draw_string(&str[5]);
            r_host->draw_string("\n");
        }
        else if (strcmp(str, "clear")) {
            r_host->reset_view();
        }

        r_host->draw_string(drv_str);
        return;
    }

    ConHost::ConHost() {
        conhosts[total_hosts] = this;
        cohost_id = total_hosts++;
        cur_input = new char[MAX_TERMINAL_TEXT];
        memset(cur_input, 0, MAX_TERMINAL_TEXT);
        return;
    }

    void ConHost::draw_string(const char *str, COL colour) {
        while (*str != 0) {
            if (*str == '\n') {
                raw_y++;
                raw_x = 0;
                off_x = 0;
                ++str;
                continue;
            }

            if (*str != ' ') 
                draw_char(*str, (off_x + raw_x) * Font::WIDTH, (off_y + raw_y) * Font::HEIGHT, colour);
            raw_x++;
            ++str;
        }

        flip_buffer();
    }

    void ConHost::send_input(char c) {
        if (c == '\n') {
            ++off_y;
            off_x = 0;
            raw_x = 0;
            ProcessCommand(cur_input);
            memset(cur_input, 0, MAX_TERMINAL_TEXT);
            return;
        }

        if (c == '\b') {
            if (off_x == 0) return;
            --off_x;
            cur_input[off_x] = 0;
            draw_char(' ', (off_x + raw_x) * Font::WIDTH, (off_y + raw_y) * Font::HEIGHT, COL::BLACK);
            flip_buffer();
            return;
        }

        if (off_x >= MAX_TERMINAL_TEXT) {
            Debug::krnl_print("TTY", Debug::LOG_INFO, "Max text length reached!");
            return;
        }

        cur_input[off_x] = c;
        if (c != ' ')
            draw_char(c, (off_x + raw_x) * Font::WIDTH, (off_y + raw_y) * Font::HEIGHT, COL::WHITE);
        ++off_x;
        flip_buffer();
    }

    void ConHost::reset_view() {
        // clear the screen
        fill_screen(COL::BLACK);
        off_x = 0;
        off_y = 0;
        raw_x = 0;
        raw_y = 0;
    }
    
    void ConHost::worker() {
        Debug::krnl_print("TTY", Debug::LOG_INFO, "ConHost %i worker begin", cohost_id);
        fill_screen(COL::BLACK);
        draw_string("Welcome", COL::GREEN);
        draw_string(" to ZyOS!\n");
        draw_string(drv_str);
        flip_buffer();
        contask->core_pinned = false;
        for (;;) {
            if (!contask) continue;
            contask->block(Scheduler::BlockReasons::AWAIT_KEYBOARD_INPUT);
        }
    }
}