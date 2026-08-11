/**
    This is a ring 0 GUI driver because I want to make a UI. This is fun. Lumina is the "proper" display ~~driver~~ server
*/
#include <DRIVER.hpp>
#include <SERVICES.hpp>
#include <TTY.hpp>
#include <LOG.hpp>
#include <HAL.hpp>
#include <lib/regs.h>

#include "Composer/Composer.hpp"
#include "Composer/Members.hpp"

namespace R0UI {
    uint64_t on_enter(Scheduler::Task *nt) {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "%s is trying to open a link with me!", nt->task_name.c_str());

        return 0;
    }

    uint64_t on_exit(Scheduler::Task *nt) {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "%s is shutting down the link!", nt->task_name.c_str());

        return 0;
    }

    uint64_t on_call(Scheduler::Task *from, uint64_t data) {
        Debug::krnl_print(
            "R0UI", 
            Debug::LOG_INFO, 
            "Received call id %i from %s",
            data,
            from->task_name.c_str()
        );

        if (data == 0) {
            Composer::do_run_through();
        }
        if (data == 1) {
            Window *nwin = new Window({{10, 10}, 200, 200});
            return (uint64_t)nwin->map_to(from);
        }

        return 0;
    }

    int main() {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Yes i run!");
        TTY::possess_host(0);
        TTY::hook_callback(0, TTY::Callback::KEYBOARD_INPUT, Composer::handle_input);

        Scheduler::Task *self_task = (Scheduler::Task *)HAL::CORE::get_core_data()->current_task;
        self_task->task_name = "R0UI_W1";

        // register an ioctl
        IPC::drvio *new_io = new IPC::drvio("R0UI/");
        new_io->on_entry = on_enter;
        new_io->on_call = on_call;
        new_io->on_exit = on_exit;

        Scheduler::Yield();

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "ints are %s", (is_interrupt_enabled()) ? "on" : "off");
        new Scheduler::Task([](void *x) {
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "well this worked?");
            ELF::Runway((const char *)x);
        }, "Test Program", true, (void *)"A:/USER/TEST_PRG.ZYX");

        Composer::worker1(TTY::get_tty_bbuffer());
        Scheduler::Suicide();
        for (;;);

        return -1;
    }
}

module_init(R0UI::main)