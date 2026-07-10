/**
    This is a ring 0 GUI driver because I want to make a UI. This is fun. Lumina is the "proper" display ~~driver~~ server
*/
#include <DRIVER.hpp>
#include <SERVICES.hpp>
#include <TTY.hpp>
#include <HAL.hpp>

#include "Composer/Composer.hpp"

namespace R0UI {
    int main() {
        uint32_t *scr_bbuff = TTY::get_tty_bbuffer();
        new Scheduler::Task([](void *tbuf){
            for (;;) Composer::worker2((uint32_t *)tbuf);
        }, "R0UI_W2",  true, scr_bbuff);

        TTY::hook_callback(0, TTY::Callback::KEYBOARD_INPUT, Composer::handle_input);

        for (;;) Composer::worker1();
    }
}

module_init(R0UI::main)