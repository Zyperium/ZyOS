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
        void draw_string(const char *str, HAL::SCREEN::COL colour = HAL::SCREEN::COL::WHITE);
        void reset_view();
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
}