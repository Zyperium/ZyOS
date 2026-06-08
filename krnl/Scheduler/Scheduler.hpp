#pragma once
#include <Library/ZyOS.hpp>
#include <Library/cystr.hpp>

namespace Scheduler {
    enum class BlockReasons {
        SLEEP,
        AWAIT_FILE_IO,
        AWAIT_MOUSE_CURSOR,
        AWAIT_KEYBOARD_INPUT,
        GARBAGE
    };

    struct TaskBlock {
        BlockReasons reason;
        uint64_t arg1;
        TaskBlock *next;
    };

    class alignas(ZyOS::sbQWORD) Task {
    public:
        using EntryPoint = void(*)(void*);
        Task();
        Task(EntryPoint entry, lib::string name);
        
        Task *next;
        Task *previous;

        lib::string task_name;

        ZyOS::QWORD rsp;
        ZyOS::QWORD pid;
        ZyOS::QWORD heap_ptr;
        ZyOS::QWORD mapped_limit;
        ZyOS::QWORD cr3;
        ZyOS::QWORD fs_base;

        ZyOS::QWORD *usr_stack_top;
        ZyOS::QWORD *krnl_stack_top;
        ZyOS::QWORD *krnl_stack_btm;
        
        ZyOS::QWORD quantum;

        ZyOS::WORD current_core;
        ZyOS::WORD lowest_queue;
        ZyOS::WORD highest_queue;

        void yield();
        void block(BlockReasons reason, ZyOS::QWORD arg = 0);
        void suicide();
        ZyOS::QWORD get_pid();
        void fork();
        // try not to call this externally.
        void change_queue(ZyOS::DWORD to);

        static void TerminateTask(Task *term);
    private:
        alignas(16) uint8_t *fx_state;
        TaskBlock *block_while;
        ZyOS::WORD current_queue;

        void enqueue(ZyOS::WORD queue_id);
        void dequeue();
    };

    extern Task last_to_yield;
    void EnableScheduler();
    void DisabledScheduler();
    void Initialize();

    constexpr ZyOS::QWORD TASK_TABLE_SIZE = 1024;
    constexpr ZyOS::DWORD TASK_DIR_SIZE = 32;
    extern Task ***TaskDirectory;

    Task *GetTaskByPID(ZyOS::QWORD PID);

    constexpr uint8_t TASK_STACK_PAGES = 8; // 8 * 4096 = 32KB of ram. Plenty.
    constexpr uint8_t TOTAL_SCHD_QUEUES = 32;
}