#include "Library/debug.hpp"
#include <TTY/TTY.hpp>

#include <HAL/SCREEN/Screen.hpp>

using namespace HAL::SCREEN;

namespace TTY {
    ConHost *conhosts[MAX_VIRTUAL_CONSHOSTS];
    size_t active_host = 0;
    size_t total_hosts = 0;

    ConHost::ConHost() {
        conhosts[total_hosts] = this;
        cohost_id = total_hosts++;
        return;
    }

    void ConHost::send_input(char c) {
        (void)c;
    }

    void ConHost::reset_view() {
        // clear the screen
    }
    
    void ConHost::worker() {
        Debug::krnl_print("TTY", Debug::LOG_INFO, "ConHost %i worker begin", cohost_id);
        for (;;) {
            if (!contask) continue;
            
        }
    }
}