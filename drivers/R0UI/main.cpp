/**
    This is a ring 0 GUI driver because I want to make a UI. This is fun. Lumina is the "proper" windowing server
*/
#include <DRIVER.hpp>
#include <SERVICES.hpp>
#include <TTY.hpp>
#include <LOG.hpp>
#include <HAL.hpp>
#include <lib/regs.h>
#include <lib/umap.hpp>

#include "Composer/Composer.hpp"
#include "Composer/Members.hpp"
#include "lib/vec.hpp"

namespace R0UI {
    lib::umap<Scheduler::Task *, lib::vec<Window *>> owned_resources;

    uint64_t on_enter(Scheduler::Task *nt) {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "%s is trying to open a link with me!", nt->task_name.c_str());
        (void)owned_resources[nt].reserve(1);
        
        return 0;
    }

    uint64_t on_exit(Scheduler::Task *nt) {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "%s is shutting down the link!", nt->task_name.c_str());
        
        lib::vec<Window *> *owr = owned_resources.find(nt);


        if (!owr) {
            Debug::krnl_print("R0UI", Debug::LOG_WARN, "Unable to find registered owned resources?");
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Did the deleted task actually use this service?");
            return 1;
        }

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Cleaning up registered task. (%x)", owr);

        for (auto i{0uz}; i < owr->size(); ++i) {
            delete owr->data()[i];
        }

        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Task cleaned.");

        owned_resources.remove(nt);
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
        else if (data == 1) {
            Window *nwin = new Window({{10, 10}, 200, 200});
            (void)owned_resources[from].push_back(nwin);
            return (uint64_t)nwin->map_to(from);
        }
        else if (data == 2) {
            lib::vec<Window *> *owr = owned_resources.find(from);
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