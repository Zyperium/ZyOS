#pragma once
#include <Scheduler/Scheduler.hpp>
#include <HAL/SCREEN/Screen.hpp>
#include <stdint.h>
#include <stddef.h>
#include <Library/cystr.hpp>

namespace TTY {
    class ConHost {
    public:
        ConHost();

        void send_input(char c);
        void early_drive_swap(char c);
        void draw_string(const char *str, HAL::SCREEN::COL colour = HAL::SCREEN::COL::WHITE);
        void reset_view();
        void check_scroll_scr();
        char _ltrdrive;
        lib::string current_wd = "/";

        void worker();
        void print_cwd();

        volatile Scheduler::Task *contask{};

    protected:
        void evaluate_command();
        size_t cohost_id = -1;
        lib::string cur_input;
    };

    extern ConHost *conhosts[];

    extern size_t total_hosts;
    extern size_t active_host;
    constexpr uint8_t MAX_VIRTUAL_CONSHOSTS = 3;
    constexpr uint32_t SCREEN_PADDING = 30;
    constexpr uint32_t SCREEN_SCROLL_Y = 8;

    constexpr uint16_t DRIVE_CMD_LEN = 3;
    constexpr uint16_t SWAPD_LEN = 8;
    constexpr uint16_t SWAPD_LETTER_OFFSET = 6;
}