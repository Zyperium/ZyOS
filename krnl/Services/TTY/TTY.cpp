#include <Services/Scheduler/Scheduler.hpp>
#include <Library/debug.hpp>
#include <Library/string.h>
#include <Library/io.hpp>
#include <Services/TTY/TTY.hpp>
#include <Services/TTY/BootTTY.hpp>
#include <Services/TTY/Commands.hpp>

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
    size_t scr_height = 0;
    size_t scr_width = 0;

    inline bool is_typeable_char(char ch) {
        bool fact = ch >= 33 && ch <= 126;
        fact = fact || ch == ' ';
        fact = fact || ch == '\n';
        fact = fact || ch == '\b';
        return fact;
    }

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

    void possess_host(int tty_id) {
        if (!conhosts[tty_id])
            return;

        conhosts[tty_id]->possessed = true;
    }

    void hook_callback(int tty_id, Callback cb, void (*func_back)(uint64_t)) {
        switch (cb) {
        case Callback::KEYBOARD_INPUT:
            conhosts[tty_id]->keyboard_callback = func_back;
            break;
        default:
            Debug::krnl_print("TTY", Debug::LOG_WARN, "Unknown callback?!");
        }
    }

    void proc_screen_ctl(size_t tty_id, ScreenCTL control, uint64_t buf) {
        if (tty_id != active_host) return;
        switch(control) {
        case ScreenCTL::SET_COL:
            HAL::SCREEN::fill_screen((COL)buf);
            break;
        case ScreenCTL::PUT_PIXEL:
            Debug::krnl_print("TTY", Debug::LOG_WARN, "Unimplemented!");
            break;
        case ScreenCTL::SWAP_BUFFER:
            HAL::SCREEN::flip_buffer();
            break;
        }
    }

    void ConHost::early_drive_swap(char c) {
        reset_view();
        draw_string("Welcome", COL::GREEN);
        draw_string(" to ZyOS!\n");
        _ltrdrive = c;
        print_cwd();
    }

    void ProcessCommand(const char *str) {
        if (strlen(str) == DRIVE_CMD_LEN) {
            if (str[1] == ':' && str[2] == '/') {
                char arg[SWAPD_LEN] = "swapd \0";
                arg[SWAPD_LETTER_OFFSET] = *str;
                ConHost *r_host = conhosts[active_host];
                r_host->draw_string(Commands::evaluate_cmd(arg).c_str());
                r_host->print_cwd();

                Debug::krnl_print("TTY", Debug::LOG_INFO, "Created string %s from %s", arg, str);
                return;
            }
        }

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

    void ConHost::check_scroll_scr() {
        if (off_y * Font::HEIGHT < scr_height - SCREEN_PADDING) {
            return;
        }

        scroll_screen(0, SCREEN_SCROLL_Y * Font::HEIGHT);
        off_y -= SCREEN_SCROLL_Y;
    }

    void ConHost::draw_string(const char *str, COL colour) {
        while (*str != 0) {
            if (*str == '\n') {
                ++off_y;
                raw_x = 0;
                off_x = 0;
                ++str;
                check_scroll_scr();
                continue;
            }

            if (*str != ' ') 
                draw_char(*str, (off_x + raw_x) * Font::WIDTH, off_y * Font::HEIGHT, colour);
            raw_x++;
            ++str;
        }

        flip_buffer();
    }

    void ConHost::send_input(char c) {
        if (possessed) {
            if (keyboard_callback)
                keyboard_callback(c);
            return;
        }

        if (!is_typeable_char(c)) return;
        if (c == '\n') {
            ++off_y;
            check_scroll_scr();
            off_x = 0;
            raw_x = 0;
            ProcessCommand(cur_input);
            memset(cur_input, 0, MAX_CONSOLE_INPUT);
            return;
        }

        if (c == '\b') {
            if (off_x == 0) return;
            --off_x;
            cur_input[off_x] = 0;
            draw_char(' ', (off_x + raw_x) * Font::WIDTH, off_y * Font::HEIGHT, COL::BLACK);
            flip_buffer();
            return;
        }

        cur_input[off_x] = c;
        if (c != ' ')
            draw_char(c, (off_x + raw_x) * Font::WIDTH, off_y * Font::HEIGHT, COL::WHITE);
        ++off_x;
        flip_buffer();
    }

    void ConHost::reset_view() {
        // clear the screen
        fill_screen(COL::BLACK);
        off_x = 0;
        off_y = 0;
        raw_x = 0;
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

        cur_input = new char[MAX_CONSOLE_INPUT];

        screen_dim dim = get_dim();
        scr_height = dim.height;
        scr_width = dim.width;

        if (!contask) {
            Debug::krnl_print("TTY", Debug::LOG_WARN, "TTY doesn't know it's own task!");
            asm volatile("sti");
            Scheduler::Yield();
            while (!contask) asm volatile("pause");
        }

        contask->niceness = 4;

        Debug::krnl_print("TTY", Debug::LOG_INFO, "Initialized");
        contask->core_pinned = false;
        asm volatile("sti");
        for (;;) {
            if (!contask) continue;
            // contask->block(Scheduler::BlockReasons::AWAIT_KEYBOARD_INPUT);

            // Why manually poll? Idk. QEMU refuses to call the IDT responsible for the handler, however...
            // The interrupts work flawlessly on real hardware. So QEMU quirk ig.
            uint8_t ps2_status = inb(0x64); 
            if (ps2_status & 0x01) {
                PS2::Keyboard::HandleInterrupt();
            }
            else {
                Scheduler::Yield();
            }
        }
    }
}