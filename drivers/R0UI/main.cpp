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
    lib::umap<lib::string, Window *> class_mapping(16);

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
        } else {
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Cleaning up registered task. (%x)", owr);

            for (auto i{0uz}; i < owr->size(); ++i) {
                class_mapping.remove(owr->data()[i]->classname);
                delete owr->data()[i];
            }

            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Task cleaned.");

            owned_resources.remove(nt);
        }

        lib::vec<Window *> *watching = watched_resources.find(nt);
        if (watching) {
            for (auto i{0uz}; i < watching->size(); ++i) {
                Window *w = watching->data()[i];
                if (w) w->remove_watcher(nt);
            }
            watched_resources.remove(nt);
        }

        return 0;
    }

    uint64_t on_call(Scheduler::Task *from, uint64_t data, uint64_t extra) {
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
            char *res = Syscalls::usr_to_string(extra, 22); // 22 for SSO

            if (res[0] == 0) {
                delete[] res;
                return 0;
            }

            if (class_mapping.contains(res)) {
                delete[] res;
                return 0;
            }

            Window *nwin = new Window({20, 20, 200, 200});

            class_mapping[res] = nwin;
            nwin->classname = res;
            delete[] res;
            (void)owned_resources[from].push_back(nwin);
            Debug::krnl_print("R0UI", Debug::LOG_INFO, "Mapping new window to %s", from->task_name.c_str());
            return (uint64_t)nwin->map_to(from);
        }
        else if (data == 2) {
            lib::vec<Window *> *owr = owned_resources.find(from);
            if (!owr) {
                Debug::krnl_print("R0UI", Debug::LOG_WARN, "Unable to find registered owned resources?");
                Debug::krnl_print("R0UI", Debug::LOG_INFO, "Did callee task actually use this service?");
                return 1;
            }

            for (auto i{0uz}; i < owr->size(); ++i) {
                owr->data()[i]->readref(from);
            }
        }
        else if (data == 3) {
            char *res = Syscalls::usr_to_string(extra, 22); // 22 for SSO

            if (res[0] == 0) {
                delete[] res;
                return 0;
            }

            Window **target = class_mapping.find(res);
            delete[] res;

            if (!target) {
                Debug::krnl_print("R0UI", Debug::LOG_INFO, "WatchWindow: no such window class");
                return 0;
            }

            WindowView *view = (*target)->watch(from);
            if (!view) {
                Debug::krnl_print("R0UI", Debug::LOG_WARN, "WatchWindow: failed to map view for %s", from->task_name.c_str());
                return 0;
            }

            return (uint64_t)view;
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

        Composer::worker1(HAL::SCREEN::fetch_buffer());
        Scheduler::Suicide();
        for (;;);

        return -1;
    }
}

module_init(R0UI::main)