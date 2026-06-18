#include <Services/ELF/ELF.hpp>
#include <Services/ELF/User.hpp>
#include <Services/Scheduler/Scheduler.hpp>

#include <HAL/MEM/VMM.hpp>
#include <HAL/MEM/KMEM.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/CORE/Core.hpp>

#include <Library/cystr.hpp>
#include <Library/string.h>
#include <Library/regs.h>
#include <Library/debug.hpp>

using namespace HAL;
using namespace MEM;

extern "C" void R3Entry(
    uint64_t entry_point,
    uint64_t fs_base,
    uint64_t r3_rsp
);

namespace ELF {
    void Runway(lib::string cmd_line) {
        Debug::krnl_print("RNWY", Debug::LOG_INFO, "Building ring 3 process");

        Scheduler::Task *task = CORE::get_thread_data()->current_task;

        uint64_t _cr3 = read_cr3();
        task->cr3 = VMM::CreateProcessPageTable(_cr3);
        
        asm volatile("mov %0, %%cr3" :: "r"(task->cr3) : "memory");

        void *entry = load_elf(cmd_line);

        auto fail_load = [&](int stage){
            Debug::krnl_print("RNWY", Debug::LOG_WARN, "Failed to load app: %s, stage: %i", cmd_line.c_str(), stage);
            Scheduler::Suicide();
            for (;;);
        };

        if (!entry) {
            fail_load(0);
        }

        auto active_pml4{(uint64_t *)(task->cr3 + PMM::hhdm_offset)};

        for (auto offset{0uz}; offset < USER::STACK_SIZE; offset += PAGE_SIZE) {
            auto ppage = PMM::alloc_page();
            if (!ppage) {
                fail_load(1);
            }

            auto virt_view{(uint64_t)ppage + PMM::hhdm_offset};
            memset((void *)virt_view, 0, PAGE_SIZE);

            VMM::map_page(active_pml4, USER::STACK_BASE + offset, (uint64_t)ppage, USER::STACK_FLAGS);
        }

        auto tls_phys_page = PMM::alloc_page();
        VMM::map_page(active_pml4, USER::TLS_BASE_ADDR, (uint64_t)tls_phys_page, USER::STACK_FLAGS);

        auto tls_virt = (uint64_t)tls_phys_page + PMM::hhdm_offset;
        memset((void *)tls_virt, 0, PAGE_SIZE);

        auto fs_base = USER::TLS_BASE_ADDR + USER::FS_BASE_OFFSET;

        auto tcb_ptr = (uint64_t *)(tls_virt + USER::FS_BASE_OFFSET);
        *tcb_ptr = fs_base;

        Debug::krnl_print("RNWY", Debug::LOG_INFO, "Finished ring 3 bootstrap");

        R3Entry((uint64_t)entry, fs_base, USER::STACK_BASE + USER::STACK_SIZE);

        Debug::krnl_print("RNWY", Debug::LOG_ERROR, "Enter ring 3 returned!?");

        Scheduler::Suicide();
        for (;;);
    }
}