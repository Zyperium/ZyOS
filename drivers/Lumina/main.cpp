#include <LOG.hpp>
#include <HAL.hpp>
#include <DRIVER.hpp>
#include <TTY.hpp>
#include <stdint.h>

namespace Lumina {
    constexpr uint64_t TTY_POACH = 0;
    void input_callback(uint64_t pass) {
        (void)pass;
        // this should actually do something
    }

    int main() {
        TTY::possess_host(TTY_POACH);

        TTY::hook_callback(TTY_POACH, TTY::Callback::KEYBOARD_INPUT, Lumina::input_callback);
        
        for (;;);
        return 0;
    }
}

module_init(Lumina::main)