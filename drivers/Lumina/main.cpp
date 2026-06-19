#include <LOG.hpp>
#include <HAL.hpp>
#include <DRIVER.hpp>
#include <TTY.hpp>
#include <lib/str.hpp>
#include <stdint.h>

using namespace HAL;
using namespace MEM;

namespace Lumina {
    constexpr uint64_t TTY_POACH = 0;
    void input_callback(uint64_t pass) {
        (void)pass;
        // this should actually do something
    }

    int main() {
        TTY::possess_host(TTY_POACH);

        TTY::hook_callback(TTY_POACH, TTY::Callback::KEYBOARD_INPUT, Lumina::input_callback);

        Debug::krnl_print("LUM", Debug::LOG_INFO, "Loading ring 3 compositor");

        

        for (;;);
        return 0;
    }
}

module_init(Lumina::main)