#pragma once
#include <Library/ZyOS.hpp>
#include <Library/cystr.hpp>

namespace Scheduler {
    enum class BlockReasons {
        SLEEP,
        AWAIT_FILE_IO,
        AWAIT_MOUSE_CURSOR,
        AWAIT_KEYBOARD_INPUT,
        GARBAGE,
        AWAIT_MSIX_EVENT,
        TOTAL_REASONS // This should always be last
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
        Task(EntryPoint entry, lib::string name, bool enqueue = true);
        
        Task *next;
        Task *previous;

        lib::string task_name;

        ZyOS::QWORD rsp;
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
        bool running;

        void block(BlockReasons reason, ZyOS::QWORD arg = 0);
        void unblock(BlockReasons unreason);
        void suicide();
        ZyOS::QWORD get_pid();
        void fork();
        // try not to call this externally.
        void change_queue(ZyOS::DWORD to);

        static void TerminateTask(Task *term);
        static void UnblockAll(BlockReasons whoisblocking);
    private:
        alignas(16) uint8_t *fx_state;
        TaskBlock *block_while;
        ZyOS::WORD current_queue;
        ZyOS::QWORD pid;

        void enqueue(ZyOS::WORD queue_id);
        void dequeue();
    };

    extern Task *last_to_yield;
    void EnableScheduler();
    void DisabledScheduler();
    void Initialize();
    void Yield();

    constexpr ZyOS::QWORD TASK_TABLE_SIZE = 1024;
    constexpr ZyOS::DWORD TASK_DIR_SIZE = 32;
    extern Task ***TaskDirectory;

    Task *GetTaskByPID(ZyOS::QWORD PID);
    void RegisterSystemIdleTask(Task *task);
    Task *StealCoCoreTask();

    constexpr uint8_t TASK_STACK_PAGES = 8; // 8 * 4096 = 32KB of ram. Plenty.
    constexpr uint8_t TOTAL_SCHD_QUEUES = 32;
    constexpr uint8_t DEFAULT_SCHD_QUEUE = 0;
}