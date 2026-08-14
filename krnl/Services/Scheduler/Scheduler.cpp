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

    lib::RB_Tree *task_tree;
    lib::Spinlock Task::lock{};
    TaskBlock *blocked_queue[(size_t)BlockReasons::TOTAL_REASONS]{};

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
    }

    void DisabledScheduler() {
        active = false;
    }

    void PollBlockedTasks() {

    }

    struct TrapFrame {
        uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
        uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
        uint64_t rsp;
        uint64_t ss;
    };

    Task *frkr_task;
    void ForkerTask() {
        frkr_task = HAL::CORE::get_core_data()->current_task;

        for (;;) {
            if (!blocked_queue[(int)BlockReasons::FORK]) {
                frkr_task->block(BlockReasons::SLEEP);
                continue;
            }

            asm volatile("cli");

            Task *to_fork = blocked_queue[(int)BlockReasons::FORK]->t_ptr;

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

            TrapFrame *child_frame = reinterpret_cast<TrapFrame *>(fork->rsp);
            Debug::krnl_print("FRKR", Debug::LOG_INFO, 
                "Child Kernel RSP: %x | Trapped RIP: %x | Trapped RSP: %x", 
                fork->rsp, child_frame->rip, child_frame->rsp
            );

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

        task_tree = new lib::RB_Tree{};

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
        lib::RB_Base* leftmost_node = task_tree->get_leftmost();
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
        cr3 = read_cr3();
        fs_base = 0;
        usr_stack_top = 0;
        krnl_stack_top = 0;
        HAL::CORE::CoreLocal *tdata = HAL::CORE::get_core_data();
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Fetching core info {Core data @ %x}", tdata);
        current_core = tdata->core_id;
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "ints are %s", (is_interrupt_enabled()) ? "on" : "off");
        current_queue = 0;
        running = false;
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
        *mxcsr = 0x1F80;
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Assigning task null name");
        task_name = "unnamed task";
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Finished primitive task setup");
    }

    Task::Task(EntryPoint entry, lib::string name, bool add_queue, void *p_arg) : Task() {
        task_name = name;
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Creating new task %s", task_name.c_str());

        krnl_stack_btm = (ZyOS::QWORD *)PMEM::alloc_pages(TASK_STACK_PAGES + 1, VMM::PTE_WRITABLE | VMM::PTE_PRESENT);
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Allocated pages for new task");

        VMM::unmap_page(
            reinterpret_cast<uint64_t *>(read_cr3() & VMM::PTE_ADDR_MASK), 
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
        reaper_task->unblock(BlockReasons::SLEEP);
        block(BlockReasons::GARBAGE);
        for (;;) asm volatile("hlt");
    }
    
    ZyOS::QWORD Task::get_pid() {
        return pid;
    }

    void Task::block(BlockReasons reason, uint64_t arg1) {
        // if (this == HAL::CORE::get_core_data()->current_task) {
        //     asm volatile("sti");
        // }

        if (blockmap[(size_t)reason]) {
            Yield();
            return;
        }

        blockmap[(size_t)reason] = true;

        if (!running)
            dequeue();

        TaskBlock *n_block = new TaskBlock {
            reason,
            arg1,
            this,
            nullptr,
            nullptr
        };

        TaskBlock *r_block = blocked_queue[(size_t)reason];
        running = false;

        if (!r_block) {
            n_block->next = n_block;
            n_block->prev = n_block;
            blocked_queue[(size_t)reason] = n_block;
            
            Yield();
            return;
        }

        n_block->next = r_block;
        n_block->prev = r_block->prev;
        r_block->prev = n_block;
        n_block->prev->next = n_block;
 
        Yield();
        return;
    }

    void Task::unblock(BlockReasons reason) {
        if (!blockmap[(size_t)reason]) {
            return;
        }

        blockmap[(size_t)reason] = false;
    
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Unblocked task %s", task_name.c_str());

        TaskBlock *found_self_block = blocked_queue[(size_t)reason];
        while (found_self_block->t_ptr != this) {
            found_self_block = found_self_block->next;
            if (found_self_block == blocked_queue[(size_t)reason]) {
                return;
            }
        }
        
        if (found_self_block->next == found_self_block) {
            blocked_queue[(size_t)reason] = nullptr;
        } else {
            if (found_self_block == blocked_queue[(size_t)reason]) {
                blocked_queue[(size_t)reason] = found_self_block->next;
            }
            found_self_block->prev->next = found_self_block->next;
            found_self_block->next->prev = found_self_block->prev;
        }

        bool requeue_task{true};

        for (auto i{0uz}; i < (size_t)BlockReasons::TOTAL_REASONS; ++i) {
            if (blockmap[i]) {
                requeue_task = false;
                break;
            }
        }

        if (requeue_task) {
            enqueue();
        }

        return;
    }

    void Task::UnblockAll(BlockReasons reason) {
        (void)reason;
    }

    void Task::enqueue() {
        lib::ScopedLock x(lock);
        if (vruntime < global_min_vruntime) {
            vruntime = global_min_vruntime;
        }

        task_tree->insert_node(this);
    }

    void Task::dequeue() {
        lib::ScopedLock x(lock);
        task_tree->remove_node(this);
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

    void Suicide() {
        HAL::CORE::get_core_data()->current_task->suicide();
    }
    
    void ClearGarbage() {
        TaskBlock *head = blocked_queue[(size_t)BlockReasons::GARBAGE];
        if (!head) {
            return;
        }
    
        blocked_queue[(size_t)BlockReasons::GARBAGE] = nullptr;
    
        TaskBlock *current = head;
        TaskBlock *next_node = nullptr;
        ZyOS::QWORD krnl_cr3 = read_cr3();
    
        do {
            next_node = current->next;
        
            if (current->t_ptr) {
                Task *t = current->t_ptr;
            
                ZyOS::QWORD pid = t->get_pid();
                ZyOS::QWORD dir = pid / TASK_TABLE_SIZE;
                ZyOS::QWORD idx = pid % TASK_TABLE_SIZE;
            
                if (TaskDirectory[dir]) {
                    TaskDirectory[dir][idx] = nullptr;
                }

                if (t->utask) {
                    Debug::krnl_print("SCHD", Debug::LOG_INFO, "Deleting utask %x", t->utask);
                    delete t->utask;
                    Debug::krnl_print("SCHD", Debug::LOG_INFO, "Finished deleting utask");
                }
            
                if (t->cr3 != krnl_cr3) {
                    VMM::FreeProcessPages(t->cr3);
                }
            
                if (t->krnl_stack_btm) {
                    uintptr_t original_alloc_ptr = reinterpret_cast<uintptr_t>(t->krnl_stack_btm) - PAGE_SIZE;
                    PMEM::free_pages(reinterpret_cast<void *>(original_alloc_ptr), TASK_STACK_PAGES + 1);
                }
            
                delete t;
            }
        
            delete current;
            current = next_node;
        } while (current != head);
    }
}
static inline void xsave_state(void *buffer) {
    uint32_t low = 0xFFFFFFFF;
    uint32_t high = 0xFFFFFFFF;
    
    asm volatile("xsave64 %0"
        : "=m" (*(uint8_t *)buffer)
        : "a" (low), "d" (high)
        : "memory");
}

static inline void xrstor_state(const void *buffer) {
    uint32_t low = 0xFFFFFFFF;
    uint32_t high = 0xFFFFFFFF;

    asm volatile("xrstor64 %0"
        :
        : "m" (*(uint8_t *)buffer), "a" (low), "d" (high)
        : "memory");
}

lib::Spinlock swaplock;
volatile bool log_switches = false;
uint64_t last_ram_prnt{0};
extern "C" uint64_t SchedulerSwitch(uint64_t current_rsp) {
    if (!Scheduler::active) {
        return current_rsp;
    }

    HAL::CORE::CoreLocal *thread_data = HAL::CORE::get_core_data();
    uint64_t curr_sys_time = ACPI::get_sys_time();
    if (curr_sys_time - last_ram_prnt > 30000) {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "RAM: %i/%i", PMM::used_memory, PMM::total_memory);
        last_ram_prnt = curr_sys_time;
    }

    if (thread_data->core_id != 0)
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Core %i is scheduling", thread_data->core_id);

    auto prev_task = thread_data->current_task;

    if (prev_task && prev_task != thread_data->system_idle_task) {
        prev_task->rsp = current_rsp;

        if (prev_task->running) {
            uint64_t delta_time = (curr_sys_time - thread_data->last_task_runtime) + 1;
            thread_data->last_task_runtime = curr_sys_time;

            prev_task->vruntime += delta_time;
            prev_task->last_ran_time = curr_sys_time;
            prev_task->running = false;

            prev_task->enqueue();
        }
    }

    Scheduler::Task *next_task = Scheduler::Task::GetNextTask();
    if (thread_data->core_id != 0)
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Swap to %s", next_task->task_name.c_str());
    if (next_task && next_task != thread_data->system_idle_task) {
        next_task->dequeue();
    }

    if (!next_task) {
        next_task = thread_data->system_idle_task;
    }

    HAL::CORE::set_lapic_shot(next_task->niceness);
    next_task->running = true;
    thread_data->current_task = next_task;

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


    if (log_switches || thread_data->core_id != 0)
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Swap to %s", next_task->task_name.c_str());

    return next_task->rsp;
}

extern "C" void AckInterrupt() {
    HAL::CORE::ack_lapic();
    return;
}