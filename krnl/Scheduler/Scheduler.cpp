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

extern "C" void QuietSwitch();

namespace Scheduler {
    bool active{};
    bool event_occured{};

    Task *task_queue[TOTAL_SCHD_QUEUES]{};
    Task *blocked_queue{};

    Task ***TaskDirectory;

    bool a_schd_lock = false;
    uint64_t cur_rflags = 0;
    int core_id_holder = -1;
    void aquire_lock() {
        if (core_id_holder == HAL::CORE::get_thread_data()->core_id) {
            return;
        }

        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        asm volatile("cli");
        while (__atomic_test_and_set(&a_schd_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        cur_rflags = rflags;
        core_id_holder = HAL::CORE::get_thread_data()->core_id;

        return;
    }

    void release_lock() {
        __atomic_clear(&a_schd_lock, __ATOMIC_RELEASE);
        restore_rflags(cur_rflags);
        cur_rflags = 0;
        core_id_holder = -1;
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

        TaskDirectory[0] = new Task*[TASK_TABLE_SIZE];
        return;
    }

    Task *GetNextTask() {
        return nullptr;
    }

    Task::Task() {
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

        TaskDirectory[dir_idx][tbl_idx] = this;

        heap_ptr = 0;
        mapped_limit = 0;
        cr3 = read_cr3();
        fs_base = 0;
        usr_stack_top = 0;
        krnl_stack_top = 0;
        quantum = 0;
        current_core = HAL::CORE::get_thread_data()->core_id;
        block_while = nullptr;
        current_queue = 0;
        fx_state = new uint8_t;
        task_name = "unnamed task";
    }

    Task::Task(EntryPoint entry, lib::string name) : Task() {
        task_name = name;

        krnl_stack_btm = (ZyOS::QWORD *)PMEM::alloc_pages(TASK_STACK_PAGES + 1, VMM::PTE_WRITABLE | VMM::PTE_PRESENT);

        VMM::unmap_page(
            reinterpret_cast<uint64_t *>(read_cr3() & VMM::PTE_ADDR_MASK), 
            reinterpret_cast<uint64_t>(krnl_stack_btm)
        );

        krnl_stack_btm += PAGE_SIZE;
        krnl_stack_top = krnl_stack_btm + (PAGE_SIZE * TASK_STACK_PAGES);

        memset(krnl_stack_btm, 0, TASK_STACK_PAGES * PAGE_SIZE);

        uint64_t *ktop = krnl_stack_top;
        *(--ktop) = 0x10; // Stack Segment. RPL 0
        *(--ktop) = reinterpret_cast<uint64_t>(krnl_stack_top) - sizeof(uint64_t);
        *(--ktop) = 0x202;
        *(--ktop) = 0x08;
        *(--ktop) = reinterpret_cast<uint64_t>(entry);

        aquire_lock();

        for (auto i{0uz}; i < 15; ++i) *(--ktop) = 0; // zero out the 15 registers.

        rsp = reinterpret_cast<uint64_t>(ktop);
        active = true;

        release_lock();

        if (active)
            yield();
    }

    void Task::yield() {
        QuietSwitch();
    }

    void Task::suicide() {
        block(BlockReasons::GARBAGE);
        for (;;);
    }

    void Task::block(BlockReasons reason, uint64_t arg1) {
        aquire_lock();

        dequeue();
        
        TaskBlock *blc = new TaskBlock;
        blc->reason = reason;
        blc->arg1 = arg1;
        
        if (!blocked_queue) {
            next = this;
            previous = this;
            blocked_queue = this;
            blc->next = nullptr;
            block_while = blc;
            release_lock();
            yield();
            return;
        }

        previous = blocked_queue->previous;
        blocked_queue->previous = this;
        next = blocked_queue;
        previous->next = this;

        if (!block_while) {
            blc->next = nullptr;
            block_while = blc;
            release_lock();
            yield();
            return;
        }

        blc->next = block_while;
        block_while = blc;

        release_lock();
        yield();
        return;
    }

    void Task::unblock(BlockReasons reason) {
        (void)reason;
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
        if (block_while) {
            Debug::krnl_print("SCHD", Debug::LOG_WARN, "Attempted to dequeue during block?");
            return;
        }

        aquire_lock();

        if (!previous || !next) {
            release_lock();
            return;
        }

        if (next == this) {
            task_queue[current_queue] = nullptr;
            next = nullptr;
            previous = nullptr;
            release_lock();
            return;
        }

        previous->next = next;
        next->previous = previous;

        next = nullptr;
        previous = nullptr;

        release_lock();
        return;
    }

    void Task::TerminateTask(Task *term) {
        aquire_lock();
        if (term->block_while) {
            release_lock();
            return;
        }

        release_lock();
        return;
    }

    Task *GetTaskByPID(ZyOS::QWORD pid) {
        ZyOS::QWORD dir = pid / TASK_TABLE_SIZE;
        ZyOS::QWORD idx = pid % TASK_TABLE_SIZE;

        if (!TaskDirectory[dir]) return nullptr;
        return TaskDirectory[dir][idx];
    }
}

extern "C" uint64_t SchedulerSwitch(uint64_t current_rsp) {
    if (!Scheduler::active) {
        return current_rsp;
    }

    auto next_rsp{0uz};

    return next_rsp;
}

extern "C" void AckInterrupt() {
    return;
}