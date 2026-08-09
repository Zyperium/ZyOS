#pragma once
#include <Library/umap.hpp>
#include <Library/vec.hpp>
#include <Library/cystr.hpp>

#include <Services/Scheduler/Scheduler.hpp>

namespace IPC {
    using DrvioCB = uint64_t (*)(Scheduler::Task *);
    using DrvioIR = uint64_t (*)(Scheduler::Task *, uint64_t details);

    class drvio {
    public:
        // Expose a path to your driver. E.G: "my_driver/" or "zyos/my_driver/"
        drvio(lib::string expose_dir);

        uint64_t add_relier(Scheduler::Task *new_reliee);
        void remove_relier(Scheduler::Task *unlinker);
        bool is_relier(Scheduler::Task *relier);

        DrvioCB on_entry;
        DrvioCB on_exit;
        DrvioIR on_call;

    private:
        lib::string exposee;
        lib::umap<uint64_t, lib::string> reliees;
    };

    extern lib::umap<lib::string, drvio *> regdrvrs;
}