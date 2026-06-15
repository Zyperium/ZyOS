#include <Scheduler/Scheduler.hpp>
#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/io.hpp>
#include <TTY/TTY.hpp>
#include <TTY/BootTTY.hpp>
#include <TTY/Commands.hpp>

#include <HAL/SCREEN/Screen.hpp>
#include <HAL/SCREEN/Font.hpp>
#include <HAL/DISK/Disk.hpp>
#include <HAL/IDT/IOAPIC/IOAPIC.hpp>
#include <HAL/PS2/PS2KB.hpp>

using namespace HAL::SCREEN;

namespace TTY {
    ConHost *conhosts[MAX_VIRTUAL_CONSHOSTS];
    size_t active_host = 0;
    size_t total_hosts = 0;
    size_t off_x = 0;
    size_t off_y = 0;
    size_t raw_x = 0;
    size_t raw_y = 0;
    size_t scr_height = 0;
    size_t scr_width = 0;

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


    void ProcessCommand(const char *str) {
        ConHost *r_host = conhosts[active_host];
        r_host->draw_string(Commands::evaluate_cmd(str).c_str());

        r_host->print_cwd();
        return;
    }

    ConHost::ConHost() {
        conhosts[total_hosts] = this;
        cohost_id = total_hosts++;
        return;
    }

    void ConHost::print_cwd() {
        draw_string("[");
        char t[3] = { _ltrdrive, ':', 0 };
        draw_string(t);
        draw_string(current_wd.c_str());
        draw_string("> ");
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
            ProcessCommand(cur_input.c_str());
            cur_input.clear();
            cur_input.empty();
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
        asm volatile("cli");
        BOOT::disable();
        Debug::krnl_print("TTY", Debug::LOG_INFO, "ConHost %i worker begin", cohost_id);
        fill_screen(COL::BLACK);
        draw_string("Welcome", COL::GREEN);
        draw_string(" to ZyOS!\n");
        _ltrdrive = '?';
        print_cwd();
        flip_buffer();

        screen_dim dim = get_dim();
        scr_height = dim.height;
        scr_width = dim.width;

        if (!contask) {
            Debug::krnl_print("TTY", Debug::LOG_WARN, "TTY doesn't know it's own task!");
            asm volatile("sti");
            Scheduler::Yield();
            while (!contask) asm volatile("pause");
        }

        Debug::krnl_print("TTY", Debug::LOG_INFO, "Initialized");
        contask->core_pinned = false;
        asm volatile("sti");
        for (;;) {
            if (!contask) continue;
            // contask->block(Scheduler::BlockReasons::AWAIT_KEYBOARD_INPUT);

            // Why manually poll? Idk. QEMU refuses to call the IDT responsible for the handler, however...
            // The interrupt works flawlessly on real hardware. So QEMU quirk ig.
            uint8_t ps2_status = inb(0x64); 
            if (ps2_status & 0x01) {
                PS2::Keyboard::HandleInterrupt();
            }
            else {
                asm volatile("pause"); 
            }
        }
    }
}