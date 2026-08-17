#include "Library/redblack.hpp"
#include "Services/IPC/drvio.hpp"
#include <stddef.h>
#include <stdint.h>

#include <Services/Scheduler/Scheduler.hpp>
#include <Services/ELF/ELF.hpp>
#include <Services/ELF/User.hpp>

#include <Library/regs.h>
#include <Library/string.h>
#include <Library/debug.hpp>
#include <Library/locks.hpp>
#include <Library/ZyOS.hpp>

#include <HAL/CORE/Core.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/PMM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/ACPI/ACPI.hpp>
#include <HAL/CORE/CoreLocal.hpp>

using namespace HAL::MEM;

namespace Scheduler {
    bool active{false};
    bool event_occured{};
    uint32_t xsave_pages{0};
    uint64_t krnl_cr3{0};

    lib::Spinlock Task::lock{};
    lib::Spinlock garbage_lock;
    GarbageQueue *garbage_queue;
    lib::RB_Tree sleep_tree;
    size_t sleeping_tasks;

    Task ***TaskDirectory;
    Task *reaper_task{nullptr};

    bool a_schd_lock = false;
    ZyOS::QWORD watch_pid = -1;
    ZyOS::QWORD Task::global_min_vruntime{0};

    void CheckEvents() {
        if (!event_occured) return;
    }

    void EnableScheduler() {
        active = true;
        asm volatile("sfence" ::: "memory");
    }

    void DisabledScheduler() {
        active = false;
        asm volatile("sfence" ::: "memory");
    }

    void PollBlockedTasks() {

    }

    Task *frkr_task;
    void ForkerTask() {
        frkr_task = HAL::CORE::get_core_data()->current_task;

        for (;;) {
            frkr_task->block(BlockReasons::FORK);

            asm volatile("cli");

            Task *to_fork = frkr_task;

            Debug::krnl_print("FRKR", Debug::LOG_INFO, "Forking task %s", to_fork->task_name.c_str());

            char *nname = new char[to_fork->task_name.length() + sizeof(FORK_APPEND_TEXT)];
            memcpy(nname, to_fork->task_name.c_str(), to_fork->task_name.length());
            memcpy(nname + to_fork->task_name.length(), FORK_APPEND_TEXT, sizeof(FORK_APPEND_TEXT));

            Task *fork = new Scheduler::Task(nullptr, nname, true);
            fork->block(BlockReasons::FORKD);
            delete[] nname;

            fork->utask = new UserTask();
            memcpy(fork->utask, to_fork->utask, sizeof(UserTask));
            fork->utask->task_owner = fork;

            for (auto i{0uz}; i < MAX_USR_FD; ++i) {
                if (!fork->utask->descriptors[i])
                    continue;

                fork->utask->descriptors[i]->add_ref();
            }

            fork->cr3 = VMM::ClonePageDirectory(to_fork->cr3);

            fork->heap_ptr = to_fork->heap_ptr;
            fork->mapped_limit = to_fork->mapped_limit;
            fork->fs_base = to_fork->fs_base;
            fork->niceness = to_fork->niceness;
            fork->syscalls_allowed = to_fork->syscalls_allowed;

            auto active_pml4{(uint64_t *)(fork->cr3 + PMM::hhdm_offset)};
            for (auto offset{0uz}; offset < ELF::USER::STACK_SIZE; offset += PAGE_SIZE) {
                auto ppage = PMM::alloc_page();
                if (!ppage) {
                    panic(PanicReasons::GENERAL_FAULT_KMODE);
                }

                auto virt_view{(uint64_t)ppage + PMM::hhdm_offset};
                memset((void *)virt_view, 0, PAGE_SIZE);

                VMM::map_page(active_pml4, ELF::USER::STACK_BASE + offset, (uint64_t)ppage, ELF::USER::STACK_FLAGS);
            }

            fork->usr_stack_top = reinterpret_cast<ZyOS::QWORD *>(ELF::USER::STACK_BASE + ELF::USER::STACK_SIZE);
            fork->usr_stack_save = to_fork->usr_stack_save;

            uintptr_t top_addr = reinterpret_cast<uintptr_t>(to_fork->usr_stack_top);

            for (size_t offset = 0; offset < ELF::USER::STACK_SIZE; offset += PAGE_SIZE) {
                uintptr_t page_vaddr = top_addr - PAGE_SIZE - offset;

                uint64_t src_phys = VMM::GetPhysicalAddress(to_fork->cr3, page_vaddr);
                uint64_t dst_phys = VMM::GetPhysicalAddress(fork->cr3, page_vaddr);

                if (src_phys && dst_phys) {
                    auto *src_ptr = reinterpret_cast<void *>(src_phys + PMM::hhdm_offset);
                    auto *dst_ptr = reinterpret_cast<void *>(dst_phys + PMM::hhdm_offset);
                    memcpy(dst_ptr, src_ptr, PAGE_SIZE);
                }
            }

            memcpy(fork->fx_state, to_fork->fx_state, FX_STATE_SIZE);

            memcpy(fork->krnl_stack_btm, to_fork->krnl_stack_btm, PAGE_SIZE * TASK_STACK_PAGES);

            uintptr_t parent_rsp_offset = to_fork->rsp - reinterpret_cast<uintptr_t>(to_fork->krnl_stack_btm);

            fork->rsp = reinterpret_cast<uintptr_t>(fork->krnl_stack_btm) + parent_rsp_offset;

            to_fork->unblock(BlockReasons::FORK);
            fork->unblock(BlockReasons::FORKD);
            
            asm volatile("sfence" ::: "memory");

            asm volatile("sti");

            Yield();
        }
    }

    void Initialize() {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Initialize");

        TaskDirectory = new Task**[TASK_DIR_SIZE];
        for (auto i{0uz}; i < TASK_DIR_SIZE; i++) {
            TaskDirectory[i] = nullptr;
        }

        krnl_cr3 = read_cr3();
        garbage_queue = (GarbageQueue *)PMEM::alloc_page(VMM::PTE_WRITABLE | VMM::PTE_PRESENT | VMM::PTE_NX | VMM::PTE_CACHELESS);
        memset(garbage_queue, 0, sizeof(GarbageQueue));

        HAL::CORE::get_core_data()->task_tree = new lib::RB_Tree{};

        active = false;
        TaskDirectory[0] = new Task*[TASK_TABLE_SIZE];
        for (auto i{0uz}; i < TASK_TABLE_SIZE; i++) {
            TaskDirectory[0][i] = nullptr;
        }
        return;
    }

    // This is a bit of a weird thing idk. Fix this later ig.
    Task *StealCoCoreTask() {
        return Task::GetNextTask();
    }

    Task *Task::GetNextTask() {
        asm volatile("mfence" ::: "memory");
        lib::RB_Base* leftmost_node = HAL::CORE::get_core_data()->task_tree->get_leftmost();
        if (!leftmost_node) {
            return HAL::CORE::get_core_data()->system_idle_task;
        }

        Task *pick = static_cast<Task *>(leftmost_node);
        if (pick->vruntime > MIN_VRUNTIME_OFFSET)
            global_min_vruntime = pick->vruntime - MIN_VRUNTIME_OFFSET;
        return pick;
    }

    UserTask::~UserTask() {
        for (auto i{0uz}; i < Scheduler::MAX_USR_FD; ++i) {
            if (!descriptors[i])
                continue;
            descriptors[i]->release();
        }

        for (auto i{0uz}; i < opened_drvrs.size(); ++i) {
            IPC::drvio *dvio = *IPC::regdrvrs.find(opened_drvrs[i]);

            if (!dvio) {
                Debug::krnl_print(
                    "SCHD", 
                    Debug::LOG_INFO, 
                    "Unable to find drvrio %s", 
                    opened_drvrs[i].c_str()
                );
                continue;
            }

            dvio->remove_relier(task_owner);
        }

        return;
    }

    static inline uint32_t get_xsave_size() {
        uint32_t eax, ebx, ecx, edx;
        
        asm volatile("cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0xD), "c"(0x0));

        return ebx; 
    }

    Task::Task() : vruntime(0), utask(nullptr), niceness(1), core_pinned(false), syscalls_allowed(false) {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Beginning primitive task setup");
        static size_t next_pid = 0;

        rsp = 0;
        pid = next_pid++;
        ZyOS::DWORD dir_idx = pid / TASK_TABLE_SIZE;
        ZyOS::DWORD tbl_idx = pid % TASK_TABLE_SIZE;
        if (dir_idx >= TASK_DIR_SIZE) {
            panic(PanicReasons::OUT_OF_PIDs);
        }

        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Task is being assigned PID of %i, in dir %i and table %i", pid, dir_idx, tbl_idx);
        if (!TaskDirectory[dir_idx]) {
            TaskDirectory[dir_idx] = new Task*[TASK_TABLE_SIZE];
            for (size_t i = 0; i < TASK_TABLE_SIZE; i++) {
                TaskDirectory[dir_idx][i] = nullptr;
            }
        }

        TaskDirectory[dir_idx][tbl_idx] = this;

        heap_ptr = 0;
        mapped_limit = 0;
        cr3 = krnl_cr3;
        fs_base = 0;
        usr_stack_top = 0;
        krnl_stack_top = 0;
        HAL::CORE::CoreLocal *tdata = HAL::CORE::get_core_data();
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Fetching core info {Core data @ %x}", tdata);
        current_core = tdata->core_id;
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "ints are %s", (is_interrupt_enabled()) ? "on" : "off");
        running = false;
        is_queued = false;
        if (!xsave_pages) {
            uint32_t xsve = get_xsave_size();

            xsave_pages = (xsve + (PAGE_SIZE - 1))  / PAGE_SIZE;
        }

        fx_state = (uint8_t *)PMEM::alloc_pages(
            xsave_pages,
            VMM::PTE_PRESENT |
            VMM::PTE_WRITABLE |
            VMM::PTE_NX
        );
    
        memset(fx_state, 0, FX_STATE_SIZE);
        vruntime = global_min_vruntime;

        uint32_t *mxcsr = reinterpret_cast<uint32_t *>(&fx_state[24]);
        *reinterpret_cast<uint16_t *>(&fx_state[0]) = 0x037F;
        *reinterpret_cast<uint32_t *>(&fx_state[28]) = 0x0000FFFF;
        *mxcsr = 0x1F80;
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Assigning task null name");
        task_name = "unnamed task";
        asm volatile("sfence" ::: "memory");
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Finished primitive task setup");
    }

    Task::Task(EntryPoint entry, lib::string name, bool add_queue, void *p_arg) : Task() {
        task_name = name;
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Creating new task %s", task_name.c_str());

        krnl_stack_btm = (ZyOS::QWORD *)PMEM::alloc_pages(TASK_STACK_PAGES + 1, VMM::PTE_WRITABLE | VMM::PTE_PRESENT);
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Allocated pages for new task (@%x)", krnl_stack_btm);

        VMM::unmap_page(
            reinterpret_cast<uint64_t *>(krnl_cr3 + PMM::hhdm_offset), 
            reinterpret_cast<uint64_t>(krnl_stack_btm)
        );

        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Unmapped page %x", krnl_stack_btm);

        uintptr_t btm_address = reinterpret_cast<uintptr_t>(krnl_stack_btm);
        btm_address += PAGE_SIZE;
        uintptr_t top_address = btm_address + (PAGE_SIZE * TASK_STACK_PAGES);

        krnl_stack_btm = reinterpret_cast<ZyOS::QWORD *>(btm_address);
        krnl_stack_top = reinterpret_cast<ZyOS::QWORD *>(top_address);

        memset(krnl_stack_btm, 0, TASK_STACK_PAGES * PAGE_SIZE);
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Performed memset on kernel stack");

        uint64_t *ktop = krnl_stack_top;
        *(--ktop) = 0x10; // Stack Segment. RPL 0
        *(--ktop) = reinterpret_cast<uint64_t>(krnl_stack_top) - sizeof(uint64_t);
        *(--ktop) = 0x202;
        *(--ktop) = 0x08; // Code Segment, also RPL 0. Crazy.
        *(--ktop) = reinterpret_cast<uint64_t>(entry);

        for (auto i{0uz}; i < 15; ++i) *(--ktop) = 0; // zero out the 15 registers.

        rsp = reinterpret_cast<uint64_t>(ktop);

        constexpr uint8_t RDI_OFFSET_ASM = 5;
        uint64_t *RDI_REG = &ktop[RDI_OFFSET_ASM];
        *RDI_REG = (uint64_t)p_arg;

        if (add_queue) {
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "Enqueued task!");
            enqueue();
        }

        asm volatile("sfence" ::: "memory");
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Scheduler has initialized task %s", task_name.c_str());
        return;
    }

    int64_t Task::compare(const lib::RB_Base* other) const {
        const Task *o = static_cast<const Task *>(other);
        
        if (vruntime < o->vruntime) return -1;
        if (vruntime > o->vruntime) return 1;
        
        if (pid < o->pid) return -1;
        if (pid > o->pid) return 1;

        return 0;
    }

    void Yield() {
        uint64_t _rflags;
        asm volatile("pushfq; pop %0" : "=r"(_rflags));
        asm volatile("sti\nint $0x67");
        restore_rflags(_rflags);
    }

    void Task::suicide() {
        block(BlockReasons::GARBAGE);
        for (;;) asm volatile("hlt");
    }
    
    ZyOS::QWORD Task::get_pid() {
        return pid;
    }

    void Task::block(BlockReasons reason, uint64_t arg1) {
        if (blocked == reason) {
            blocked_by = arg1;
            return;
        }

        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Blocking %s", task_name.c_str());

        if (reason == BlockReasons::GARBAGE) {
            lib::ScopedLock x(garbage_lock);
            garbage_queue->to_clear[garbage_queue->curr_ptr] = this;
            ++garbage_queue->curr_ptr;
            reaper_task->unblock(BlockReasons::SLEEP);
        }

        blocked = reason;
        blocked_by = arg1;
        
        if (!running)
            dequeue();
        
        running = false;

        asm volatile("sfence" ::: "memory");
        Yield();
        return;
    }

    lib::Spinlock block_lock;
    void Task::unblock(BlockReasons reason) {
        if (blocked != reason)
            return;

        blocked = BlockReasons::TOTAL_REASONS;

        enqueue();

        return;
    }

    void Task::UnblockAll(BlockReasons reason) {
        (void)reason;
    }

    void Task::enqueue() {
        if (is_queued) return;
        lib::ScopedLock x(lock);
        if (vruntime < global_min_vruntime) {
            vruntime = global_min_vruntime;
        }
        if (HAL::CORE::get_core_data()->core_id != 0)
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "Inserting %s into AP%i", task_name.c_str(), HAL::CORE::get_core_data()->core_id);
        HAL::CORE::get_core_data()->task_tree->insert_node(this);
        is_queued = true;
        asm volatile("sfence" ::: "memory");
        return;
    }

    void Task::dequeue() {
        if (!is_queued) return;
        lib::ScopedLock x(lock);
        HAL::CORE::get_core_data()->task_tree->remove_node(this);
        is_queued = false;
        asm volatile("sfence" ::: "memory");
    }

    void Task::TerminateTask(Task *term) {
        term->block(BlockReasons::GARBAGE);
        
        return;
    }

    Task *GetTaskByPID(ZyOS::QWORD pid) {
        ZyOS::QWORD dir = pid / TASK_TABLE_SIZE;
        ZyOS::QWORD idx = pid % TASK_TABLE_SIZE;

        if (!TaskDirectory[dir]) return nullptr;
        return TaskDirectory[dir][idx];
    }

    void Task::sleep(ZyOS::QWORD time) {
        Scheduler::sleeping_tasks++;
        vruntime = time + ACPI::get_sys_time();
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Inserting task into sleep tree!");
        sleep_tree.insert_node(this);
        block(BlockReasons::SLEEP);
    }

    void Suicide() {
        HAL::CORE::get_core_data()->current_task->suicide();
    }

    void ClearGarbage() {
        lib::ScopedLock x(garbage_lock);
    
        for (auto i{0uz}; i < garbage_queue->curr_ptr; ++i) {
            Task *t = garbage_queue->to_clear[i];
        
            ZyOS::QWORD pid = t->get_pid();
            ZyOS::QWORD dir = pid / TASK_TABLE_SIZE;
            ZyOS::QWORD idx = pid % TASK_TABLE_SIZE;
        
            if (TaskDirectory[dir]) {
                TaskDirectory[dir][idx] = nullptr;
            }
            if (t->utask) {
                delete t->utask;
            }
        
            if (t->cr3 != krnl_cr3) {
                VMM::FreeProcessPages(t->cr3);
            }
        
            if (t->krnl_stack_btm) {
                uintptr_t original_alloc_ptr = reinterpret_cast<uintptr_t>(t->krnl_stack_btm) - PAGE_SIZE;
                PMEM::free_pages(reinterpret_cast<void *>(original_alloc_ptr), TASK_STACK_PAGES + 1);
            }
        
            garbage_queue->to_clear[i] = nullptr;
            delete t;
        }

        garbage_queue->curr_ptr = 0;
    }

    lib::Spinlock steal_lock;
    Task *AttemptCoreSteal() {
        for (auto i{0uz}; i < HAL::CORE::get_core_count(); ++i) {
            if (i == (size_t)HAL::CORE::get_core_data()->core_id) continue;

            lib::RB_Tree *target = HAL::CORE::CoreTLS[i]->task_tree;

            if (target->get_node_count() <= 2) {
                continue;
            }

            lib::ScopedLock x(steal_lock);

            if (target->get_node_count() <= 2) {
                continue;
            }

            Task *stolen = (Task *)target->steal_rightmost();
            if (stolen == nullptr) {
                continue;
            }

            asm volatile("sfence" ::: "memory");
            stolen->running = true;
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "Stealing task %s", stolen->task_name.c_str());
            return stolen;
        }

        return nullptr;
    }
}

static inline void xsave_state(void *buffer) {
    uint32_t low, high;

    asm volatile("xgetbv" : "=a" (low), "=d" (high) : "c" (0));
    
    asm volatile("xsave64 %0"
        : "=m" (*(uint8_t *)buffer)
        : "a" (low), "d" (high)
        : "memory");

    asm volatile("sfence" ::: "memory");
}

static inline void xrstor_state(void *buffer) {
    uint64_t *header = reinterpret_cast<uint64_t *>(static_cast<uint8_t *>(buffer) + 512);
    uint32_t low, high;

    asm volatile("xgetbv" : "=a" (low), "=d" (high) : "c" (0));

    if ((uint64_t)buffer % 64 != 0) {
        Debug::krnl_print("SCHD", Debug::LOG_ERROR, "Misaligned address!");
        for (;;);
    }

    uint64_t xcr0 = (static_cast<uint64_t>(high) << 32) | low;
    if (header[0] & ~xcr0) {
        Debug::krnl_print("SCHD", Debug::LOG_ERROR, "xCR0 has bits not set in XSTATE_BV");
        for (;;);
    }

    if (header[1] & (1ULL << 63)) {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "xrstr on compacted buffer?");
        for (;;);
    }

    for (int i = 2; i < 8; i++) {
        if (header[i] != 0) {
            header[i] = 0;
            // Debug::krnl_print("SCHD", Debug::LOG_ERROR, "Reserved bytes in XSAVE header are non-zero!");
            asm volatile("pause");
        }
    }

    uint32_t mxcsr = *reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(buffer) + 24);
    if (mxcsr & 0xFFFF0000) {
        Debug::krnl_print("SCHD", Debug::LOG_ERROR, "MXCSR has reserved upper bits set!");
        for (;;);
    }

    asm volatile("xrstor64 %0"
        :
        : "m" (*(const uint8_t *)buffer), "a" (low), "d" (high)
        : "memory");

    asm volatile("sfence" ::: "memory");
}

void SysIdleTask();
lib::Spinlock swaplock;
uint64_t last_ram_prnt{0};
extern "C" uint64_t SchedulerSwitch(uint64_t current_rsp) {
    if (!Scheduler::active) {
        return current_rsp;
    }

    HAL::CORE::CoreLocal *thread_data = HAL::CORE::get_core_data();
    uint64_t curr_sys_time = ACPI::get_sys_time();
    if (curr_sys_time - last_ram_prnt > 10000) {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "RAM: %i/%i", PMM::used_memory, PMM::total_memory);
        last_ram_prnt = curr_sys_time;
    }

    auto prev_task = thread_data->current_task;

    Scheduler::Task *next_task = Scheduler::Task::GetNextTask();
    
    if (prev_task) {
        prev_task->rsp = current_rsp;
        if (prev_task->running && prev_task != thread_data->system_idle_task) {
            uint64_t delta_time = (curr_sys_time - thread_data->last_task_runtime) + 1;
            thread_data->last_task_runtime = curr_sys_time;

            prev_task->vruntime += delta_time;
            prev_task->last_ran_time = curr_sys_time;
            prev_task->running = false;

            // Debug::krnl_print("SCHD", Debug::LOG_INFO, "%s vruntim @ %i", prev_task->task_name.c_str(), prev_task->vruntime);

            prev_task->enqueue();
        }
    }

    if (thread_data->core_id == 0 && Scheduler::sleep_tree.get_node_count() > 0) {
        loop:
        Scheduler::Task *t = (Scheduler::Task *)Scheduler::sleep_tree.get_leftmost();
        if (t && t->vruntime >= curr_sys_time) {
            Scheduler::sleep_tree.remove_node(t);
            t->vruntime = 0; // this forces it back to the lowest
            t->unblock(Scheduler::BlockReasons::SLEEP); // reinserts it to regular tree.
            --Scheduler::sleeping_tasks;
            goto loop;
        }
        asm volatile("sfence" ::: "memory");
    }

    if (next_task && next_task != thread_data->system_idle_task) {
        next_task->dequeue();
    }

    if (!next_task) {
        next_task = thread_data->system_idle_task;
    }
    
    HAL::CORE::set_lapic_shot(next_task->niceness);
    next_task->running = true;
    thread_data->current_task = next_task;

    if (next_task == thread_data->system_idle_task) {
        Scheduler::Task *new_target = Scheduler::AttemptCoreSteal();
        if (new_target) next_task = new_target;
        // else Debug::krnl_print("SCHD", Debug::LOG_INFO, "Failed to steal task ):");
    }

    if (next_task != prev_task) {
        if (prev_task) {
            if (prev_task->cr3 != next_task->cr3) {
                asm volatile("mov %0, %%cr3" :: "r"(next_task->cr3) : "memory");
            }
            if (prev_task->fx_state) {
                xsave_state(prev_task->fx_state);
            }
        }

        if (next_task->fx_state) {
            xrstor_state(next_task->fx_state);
        }
    }

    return next_task->rsp;
}

extern "C" void AckInterrupt() {
    HAL::CORE::ack_lapic();
    return;
}