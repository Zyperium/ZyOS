#pragma once
#include <Scheduler/Scheduler.hpp>
#include <stdint.h>
#include <stddef.h>
#include <Library/cystr.hpp>

namespace TTY {
    class ConHost {
    public:
        ConHost();

        void send_input(char c);
        void reset_view();

        void worker();

        Scheduler::Task *contask{};

    protected:
        void evaluate_command();
        size_t cohost_id = -1;
        lib::string current_input;
    };

    extern ConHost *conhosts[];

    extern size_t total_hosts;
    extern size_t active_host;
    constexpr uint8_t MAX_VIRTUAL_CONSHOSTS = 3;
}