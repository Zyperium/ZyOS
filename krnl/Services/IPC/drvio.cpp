#include <Library/debug.hpp>
#include <Library/hash.hpp>

#include <HAL/CORE/Core.hpp>
#include <Services/Scheduler/Scheduler.hpp>
#include <Services/IPC/drvio.hpp>
 //rebuild x1

namespace IPC {
    lib::umap<lib::string, drvio *> regdrvrs(16);

    static volatile size_t unnamed;
    drvio::drvio(lib::string exposed_dir) {
        if (exposed_dir[exposed_dir.length() - 1] != '/' || exposed_dir.length() < 2 || regdrvrs.contains(exposed_dir)) {
            Debug::krnl_print("IPC", Debug::LOG_WARN, "Bad expose path from driver %s", HAL::CORE::get_core_data()->current_task->task_name.c_str());
            char newn[32];
            
            Debug::snprintf(newn, 32, "unknown %i", unnamed);
            exposee = newn;
            return;
        }

        exposee = exposed_dir;
        regdrvrs[exposee] = this;
        Debug::krnl_print("IPC", Debug::LOG_INFO, "Created new plug at %s (strlen %i)", exposed_dir.c_str(), exposed_dir.length());
    }

    uint64_t drvio::add_relier(Scheduler::Task *new_reliee) {
        if (!new_reliee->utask)
            return -1;

        char new_path[128];
        Debug::snprintf(new_path, 128, "%s%i", exposee.c_str(), new_reliee->get_pid());

        Debug::krnl_print("IPC", Debug::LOG_INFO, "New device %s", new_path);
        (void)new_reliee->utask->opened_drvrs.emplace_back(exposee.c_str());
        reliees[new_reliee->get_pid()] = new_path; 

        return on_entry(new_reliee);
    }

    bool drvio::is_relier(Scheduler::Task *new_reliee) {
        if (!new_reliee->utask)
            return -1;

        return reliees.contains(new_reliee->get_pid());
    }

    void drvio::remove_relier(Scheduler::Task *unlink) {
        reliees.remove(unlink->get_pid());
        on_exit(unlink);
    }
}