/**
    This is a ring 0 GUI driver because I want to make a UI. This is fun. Lumina is the "proper" display ~~driver~~ server
*/
#include <DRIVER.hpp>
#include <SERVICES.hpp>
#include <TTY.hpp>
#include <LOG.hpp>
#include <HAL.hpp>

#include "Composer/Composer.hpp"

namespace R0UI {
    int main() {
        Debug::krnl_print("CMPSR", Debug::LOG_INFO, "Yes i run!");

        TTY::hook_callback(0, TTY::Callback::KEYBOARD_INPUT, Composer::handle_input);

        Scheduler::Task *self_task = (Scheduler::Task *)HAL::CORE::get_core_data()->current_task;
        self_task->task_name = "R0UI_W1";

        Composer::worker1(TTY::get_tty_bbuffer());
        Scheduler::Suicide();
        for (;;);

        return -1;
    }
}

module_init(R0UI::main)