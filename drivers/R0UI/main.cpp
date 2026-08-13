/**
    This is a ring 0 GUI driver because I want to make a UI. This is fun. Lumina is the "proper" windowing server
    Though don't quote me on this lol. Maybe I'll just refactor this forever.
*/
#include <DRIVER.hpp>
#include <SERVICES.hpp>
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
            Composer::force_redraw();
            Composer::do_run_through();
            return 0;
        }
        else if (data == 1) {
            Window *nwin = new Window({20, 20, 200, 200});
            (void)owned_resources[from].push_back(nwin);
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Mapping new window to %s", from->task_name.c_str());
            return (uint64_t)nwin->map_to(from);
        }
        else if (data == 2) {
            lib::vec<Window *> *owr = owned_resources.find(from);
            if (!owr) {
                Debug::krnl_print("R0UI", Debug::LOG_WARN, "Unable to find registered owned resources?");
                Debug::krnl_print("R0UI", Debug::LOG_INFO, "Did the deleted task actually use this service?");
                return 1;
            }

            for (auto i{0uz}; i < owr->size(); ++i) {
                owr->data()[i]->readref(from);
            }
        }

        return 0;
    }

    int main() {
        Debug::krnl_print("R0UI", Debug::LOG_INFO, "Yes i run!");

        TTY::BOOT::disable();

        Scheduler::Task *self_task = (Scheduler::Task *)HAL::CORE::get_core_data()->current_task;
        self_task->task_name = "R0UI_W1";

        // register an ioctl
        IPC::drvio *new_io = new IPC::drvio("R0UI/");
        new_io->on_entry = on_enter;
        new_io->on_call = on_call;
        new_io->on_exit = on_exit;

        Scheduler::Yield();

        // Debug::krnl_print("R0UI", Debug::LOG_INFO, "ints are %s", (is_interrupt_enabled()) ? "on" : "off");
        // new Scheduler::Task([](void *x) {
        //     Debug::krnl_print("SCHD", Debug::LOG_INFO, "well this worked?");
        //     ELF::Runway((const char *)x);
        // }, "Test Program", true, (void *)"A:/USER/TEST_PRG.ZYX");

        Composer::worker1(HAL::SCREEN::fetch_buffer());
        Scheduler::Suicide();
        for (;;);

        return -1;
    }
}

module_init(R0UI::main)