#include <HAL/CORE/ThreadLocal.hpp>
#include <Library/ZyOS.hpp>
#include <stddef.h>
#include <stdint.h>

#include <Scheduler/Scheduler.hpp>

#include <Library/regs.h>
#include <Library/string.h>
#include <Library/debug.hpp>

#include <HAL/CORE/Core.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/IDT/Panic.hpp>

using namespace HAL::MEM;

namespace Scheduler {
    bool active{false};
    bool event_occured{};

    Task *task_queue[TOTAL_SCHD_QUEUES]{};
    TaskBlock *blocked_queue[(size_t)BlockReasons::TOTAL_REASONS]{};
    Task *last_to_yield;

    Task ***TaskDirectory;

    bool a_schd_lock = false;
    uint64_t cur_rflags = 0;
    uint64_t core_id_holder = -1;
    int reentrancy = 0;

    void aquire_lock() {
        if (HAL::CORE::get_thread_data()->current_task) {
            if (core_id_holder == HAL::CORE::get_thread_data()->current_task->get_pid()) {
                ++reentrancy;
                return;
            }
        }

        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        asm volatile("cli");
        while (__atomic_test_and_set(&a_schd_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "STUCK!");
        }

        cur_rflags = rflags;
        if (HAL::CORE::get_thread_data()->current_task)
            core_id_holder = HAL::CORE::get_thread_data()->current_task->get_pid();


        return;
    }

    void release_lock() {
        if (reentrancy > 0) {
            --reentrancy;
            return;
        }
        
        restore_rflags(cur_rflags);
        cur_rflags = 0;
        core_id_holder = -1;

        __atomic_clear(&a_schd_lock, __ATOMIC_RELEASE);
        return;
    }

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

    void Initialize() {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Initialize");

        TaskDirectory = new Task**[TASK_DIR_SIZE];
        for (auto i{0uz}; i < TASK_DIR_SIZE; i++) {
            TaskDirectory[i] = nullptr;
        }

        active = false;

        TaskDirectory[0] = new Task*[TASK_TABLE_SIZE];
        for (auto i{0uz}; i < TASK_TABLE_SIZE; i++) {
            TaskDirectory[0][i] = nullptr;
        }
        return;
    }

    Task *StealCoCoreTask() {
        for (auto i{0uz}; i < TOTAL_SCHD_QUEUES; i++) {
            if (!task_queue[i])
                continue;
            if (!task_queue[i]->running && !task_queue[i]->core_pinned) {
                task_queue[i]->current_core = HAL::CORE::get_thread_data()->core_id;
                Task *selected_task = task_queue[i];
                task_queue[i] = task_queue[i]->next;
                return selected_task;
            }
        }

        return nullptr;
    }

    Task *GetNextTask() {
        for (auto i{0uz}; i < TOTAL_SCHD_QUEUES; i++) {
            if (!task_queue[i])
                continue;

            bool only_task_iqueue = task_queue[i] == task_queue[i]->next;
        
            if (task_queue[i] == last_to_yield) {
                if (only_task_iqueue)
                    continue;
                task_queue[i] = task_queue[i]->next;
            }

            Task *starting_task = task_queue[i];
            find_workable_core:
            if (task_queue[i]->current_core != HAL::CORE::get_thread_data()->core_id) {
                if (only_task_iqueue)
                    continue;
                task_queue[i] = task_queue[i]->next;
                if (task_queue[i] == starting_task)
                    continue;
                goto find_workable_core;
            }

            Task *selected_task = task_queue[i];
            task_queue[i] = task_queue[i]->next;

            return selected_task;
        }

        return HAL::CORE::get_thread_data()->system_idle_task;
    }

    Task::Task() {
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Beginning primitive task setup");
        static size_t next_pid = 0;
        next = nullptr;
        previous = nullptr;

        rsp = 0;
        pid = next_pid++;
        ZyOS::DWORD dir_idx = pid / TASK_TABLE_SIZE;
        ZyOS::DWORD tbl_idx = pid % TASK_TABLE_SIZE;
        if (dir_idx >= TASK_DIR_SIZE) {
            panic(PanicReasons::OUT_OF_PIDs);
        }

        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Task is being assigned PID of %i, in dir %i and table %i", pid, dir_idx, tbl_idx);

        if (!TaskDirectory[dir_idx]) {
            Debug::krnl_print("SCHD", Debug::LOG_WARN, "Non-existent directory!");
        }
        else {
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "Tables & Directories verified!");
        }

        TaskDirectory[dir_idx][tbl_idx] = this;

        heap_ptr = 0;
        mapped_limit = 0;
        cr3 = read_cr3();
        fs_base = 0;
        usr_stack_top = 0;
        krnl_stack_top = 0;
        quantum = 0;
        HAL::CORE::ThreadLocal *tdata = HAL::CORE::get_thread_data();
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Fetching core info {Core data @ %x}", tdata);
        current_core = tdata->core_id;
        current_queue = 0;
        running = false;
        malignedfx = new uint8_t[FX_STATE_SIZE];
        memset(malignedfx, 0, FX_STATE_SIZE);

        auto addr = reinterpret_cast<uint64_t>(malignedfx);
        if (addr % 0x10 != 0) {
            addr = (addr + 0x10) & ~0xF;
        }

        fx_state = reinterpret_cast<uint8_t *>(addr);
        uint32_t *mxcsr = reinterpret_cast<uint32_t*>(&fx_state[24]);
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
        *(--ktop) = 0x08;
        *(--ktop) = reinterpret_cast<uint64_t>(entry);

        for (auto i{0uz}; i < 15; ++i) *(--ktop) = 0; // zero out the 15 registers.

        rsp = reinterpret_cast<uint64_t>(ktop);

        constexpr uint8_t RDI_OFFSET_ASM = 5;
        uint64_t *RDI_REG = &ktop[RDI_OFFSET_ASM];
        *RDI_REG = (uint64_t)p_arg;

        if (add_queue)
            enqueue(DEFAULT_SCHD_QUEUE);

        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Scheduler has initialized task %s", task_name.c_str());
        return;
    }

    void Yield() {
        // Debug::krnl_print("SCHD", Debug::LOG_INFO, "Yield called");
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
        aquire_lock();
        
        if (this == HAL::CORE::get_thread_data()->current_task) {
            asm volatile("sti");
        }

        if (blockmap[(size_t)reason]) {
            release_lock();
            return;
        }

        blockmap[(size_t)reason] = true;

        dequeue();

        TaskBlock *n_block = new TaskBlock {
            reason,
            arg1,
            this,
            nullptr,
            nullptr
        };


        TaskBlock *r_block = blocked_queue[(size_t)reason];
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Blocked task %s", task_name.c_str());

        if (!r_block) {
            n_block->next = n_block;
            n_block->prev = n_block;
            blocked_queue[(size_t)reason] = n_block;
            release_lock();
            Yield();
            return;
        }

        n_block->next = r_block;
        n_block->prev = r_block->prev;
        r_block->prev = n_block;
        n_block->prev->next = n_block;

        release_lock();
        Yield();
        return;
    }

    void Task::unblock(BlockReasons reason) {
        aquire_lock();
        if (!blockmap[(size_t)reason]) {

            release_lock();
            return;
        }

        blockmap[(size_t)reason] = false;
    
        Debug::krnl_print("SCHD", Debug::LOG_INFO, "Unblocked task %s", task_name.c_str());

        TaskBlock *found_self_block = blocked_queue[(size_t)reason];
        while (found_self_block->t_ptr != this) {
            found_self_block = found_self_block->next;
            if (found_self_block == blocked_queue[(size_t)reason]) {
                release_lock();
                return;
            }
        }
        
        if (found_self_block->prev == found_self_block) {
            blocked_queue[(size_t)reason] = nullptr;
            delete found_self_block;
        }
        else if (found_self_block == blocked_queue[(size_t)reason]) {
            blocked_queue[(size_t)reason] = blocked_queue[(size_t)reason]->next;
            found_self_block->prev->next = found_self_block->next;
            found_self_block->next->prev = found_self_block->prev;
            delete found_self_block;
        }

        bool requeue_task{true};

        for (auto i{0uz}; i < (size_t)BlockReasons::TOTAL_REASONS; ++i) {
            if (blockmap[i]) {
                requeue_task = false;
                break;
            }
        }

        if (requeue_task) {
            enqueue(DEFAULT_SCHD_QUEUE);
        }

        release_lock();
        return;
    }

    void Task::UnblockAll(BlockReasons reason) {
        (void)reason;
    }

    void Task::enqueue(ZyOS::WORD queue_id) {
        if (queue_id > TOTAL_SCHD_QUEUES) {
            Debug::krnl_print(
                "SCHD", Debug::LOG_WARN, 
                "Attempted to queue in non-existent queue: %i", queue_id
            );
        }

        aquire_lock();

        if (!task_queue[queue_id]) {
            next = this;
            previous = this;
            task_queue[queue_id] = this;
            current_queue = queue_id;
            release_lock();
            return;
        }

        Task *head = task_queue[queue_id];
        Task *tail = head->previous;

        next = head;
        previous = tail;
        tail->next = this;
        head->previous = this;

        current_queue = queue_id;
        release_lock();
        return;
    }

    void Task::dequeue() {
        // if (block_while) {
        //     Debug::krnl_print("SCHD", Debug::LOG_WARN, "Attempted to dequeue during block?");
        //     return;
        // }

        aquire_lock();

        if (!previous || !next) {
            release_lock();
            return;
        }

        if (current_queue == (ZyOS::WORD)-1) {
            release_lock();
            return;
        }

        if (next == this) {
            task_queue[current_queue] = nullptr;
            next = nullptr;
            previous = nullptr;
            release_lock();
            current_queue = -1;
            return;
        }

        previous->next = next;
        next->previous = previous;

        next = nullptr;
        previous = nullptr;

        current_queue = -1;

        release_lock();
        return;
    }

    void Task::TerminateTask(Task *term) {
        aquire_lock();

        term->block(BlockReasons::GARBAGE);

        release_lock();
        return;
    }

    Task *GetTaskByPID(ZyOS::QWORD pid) {
        ZyOS::QWORD dir = pid / TASK_TABLE_SIZE;
        ZyOS::QWORD idx = pid % TASK_TABLE_SIZE;

        if (!TaskDirectory[dir]) return nullptr;
        return TaskDirectory[dir][idx];
    }

    void Suicide() {
        HAL::CORE::get_thread_data()->current_task->suicide();
    }
    
    void ClearGarbage() {
        return;
        if (!blocked_queue[(size_t)BlockReasons::GARBAGE]) return;

        Task *exclude = HAL::CORE::get_thread_data()->current_task;
        TaskBlock *root_task = blocked_queue[(size_t)BlockReasons::GARBAGE];
        TaskBlock *reset_to{};

        do {
            if (root_task->t_ptr == exclude) {
                reset_to = root_task;
                root_task = root_task->next;
                Debug::krnl_print("SCHD", Debug::LOG_INFO, "Can't remove %s because its the active task!", reset_to->t_ptr->task_name.c_str());
                continue;
            }
            Debug::krnl_print("SCHD", Debug::LOG_INFO, "Cleaing up task %s", reset_to->t_ptr->task_name.c_str());

            uint32_t pid = root_task->t_ptr->get_pid();

            ZyOS::QWORD dir = pid / TASK_TABLE_SIZE;
            ZyOS::QWORD idx = pid % TASK_TABLE_SIZE;

            TaskDirectory[dir][idx] = nullptr;

            ZyOS::QWORD krnl_cr3 = read_cr3();
            if (root_task->t_ptr->cr3 != krnl_cr3) {
                VMM::FreeProcessPages(root_task->t_ptr->cr3);
            }

            if (root_task->t_ptr->krnl_stack_btm)
                PMEM::free_pages(root_task->t_ptr->krnl_stack_btm, TASK_STACK_PAGES);

            delete root_task->t_ptr;
            root_task->t_ptr = nullptr;

            root_task = root_task->next;
        } while (root_task != blocked_queue[(size_t)BlockReasons::GARBAGE]);

        if (reset_to) {
            reset_to->next = reset_to;
            reset_to->prev = reset_to;
        }

        blocked_queue[(size_t)BlockReasons::GARBAGE] = reset_to;
    }
}

extern "C" uint64_t SchedulerSwitch(uint64_t current_rsp) {
    if (!Scheduler::active) {
        return current_rsp;
    }

    // Scheduler::aquire_lock();

    Scheduler::ClearGarbage();

    HAL::CORE::ThreadLocal *thread_data = HAL::CORE::get_thread_data();
    if (thread_data->current_task) {
        thread_data->current_task->rsp = current_rsp;
    }

    auto next_rsp{0uz};
    Scheduler::Task *next_task = Scheduler::GetNextTask();
    
    next_rsp = next_task->rsp;

    auto prev_task = thread_data->current_task;
    thread_data->current_task = next_task;

    if (next_task != prev_task) {
        if (prev_task) {
            if (prev_task->cr3 != next_task->cr3) {
                asm volatile("mov %0, %%cr3" :: "r"(next_task->cr3) : "memory");
            }

            if (prev_task->fx_state) {
                asm volatile("fxsave %0" : "=m"(*prev_task->fx_state));
            }
        }

        if (next_task->fx_state) {
            asm volatile("fxrstor %0" : : "m"(*next_task->fx_state));
        }
    }

    // Scheduler::release_lock();

    return next_rsp;
}

extern "C" void AckInterrupt() {
    HAL::CORE::ack_lapic();
    return;
}