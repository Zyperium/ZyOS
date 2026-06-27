#pragma once
#include <stdint.h>
#include <lib/str.hpp>

namespace ELF {
    /*
        @brief How do I use this?
        Well, unless you want your driver task to be a ring 3 app, which I assume you don't,
        you need to spawn a *new* task with the runway as its entry point, pass then cmd_line
        through the void *argument, and then chill! Not too hard at all.,
    */
    void Runway(lib::string cmd_line);

    /*
        @note Probably don't use this, unless you need some hyper specific function.
        (Basically you want the elf in memory & patched, but not executing)
    */
    void *load_elf(lib::string path);
}

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

    class Task;
    struct TaskBlock {
        BlockReasons reason;
        uint64_t arg1;
        Task *t_ptr;
        TaskBlock *next;
        TaskBlock *prev;
    };

    // IF you don't know what it does, do not touch it!
    // Which I hope you *do* know what it does.
    // You will need this to fork processes, because it isn't easy!
    // The fork function does nothing btw. (:
    class alignas(64) Task {
    public:
        using EntryPoint = void(*)(void*);
        Task();
        Task(EntryPoint entry, lib::string name, bool enqueue = true, void *p_arg = nullptr);

        Task *next;
        Task *previous;

        lib::string task_name;

        uint64_t rsp;
        uint64_t heap_ptr;
        uint64_t mapped_limit;
        uint64_t cr3;
        uint64_t fs_base;

        uint64_t *usr_stack_top;
        uint64_t *krnl_stack_top;
        uint64_t *krnl_stack_btm;
        uint64_t usr_stack_save;

        uint16_t current_core;
        uint16_t lowest_queue;
        uint16_t highest_queue;
        alignas(16) uint8_t *fx_state;
        uint8_t *malignedfx;
        bool running;
        bool core_pinned{};

        void block(BlockReasons reason, uint64_t arg = 0);
        void unblock(BlockReasons unreason);
        void suicide();
        uint64_t get_pid();
        void fork();
        // try not to call this externally. (but you can)
        void change_queue(uint32_t to);

        static void TerminateTask(Task *term);
        static void UnblockAll(BlockReasons whoisblocking);
    private:
        uint16_t current_queue;
        uint64_t pid;
        bool blockmap[(size_t)BlockReasons::TOTAL_REASONS]{false};
        void *_arg;

        void enqueue(uint16_t queue_idx);
        void dequeue();
    };

    extern Task *last_to_yield;
    void EnableScheduler();
    void DisabledScheduler();
    void Initialize();
    void Yield();
    void Suicide();

    constexpr uint64_t TASK_TABLE_SIZE = 1024;
    constexpr uint32_t TASK_DIR_SIZE = 32;
    constexpr uint32_t FX_STATE_SIZE = 0x200 + 0x10;
    extern Task ***TaskDirectory;
    extern uint64_t watch_pid;

    Task *GetTaskByPID(uint64_t PID);
    void RegisterSystemIdleTask(Task *task);
    Task *StealCoCoreTask();

    constexpr uint8_t TASK_STACK_PAGES = 8; // 8 * 4096 = 32KB of ram. Plenty.
    constexpr uint8_t TOTAL_SCHD_QUEUES = 32;
    constexpr uint8_t DEFAULT_SCHD_QUEUE = 0;
}