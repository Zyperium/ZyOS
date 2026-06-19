#pragma once
#include <Library/ZyOS.hpp>
#include <Library/cystr.hpp>
#include <Library/redblack.hpp>

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

    class alignas(ZyOS::sbQWORD) Task : public lib::RB_Base {
    public:
        using EntryPoint = void(*)(void*);
        Task();
        Task(EntryPoint entry, lib::string name, bool enqueue = true, void *p_arg = nullptr);

        lib::string task_name;

        ZyOS::QWORD rsp;
        ZyOS::QWORD heap_ptr;
        ZyOS::QWORD mapped_limit;
        ZyOS::QWORD cr3;
        ZyOS::QWORD fs_base;
        ZyOS::QWORD vruntime;

        ZyOS::QWORD *usr_stack_top;
        ZyOS::QWORD *krnl_stack_top; // Used by SysEntry.asm
        ZyOS::QWORD *krnl_stack_btm;
        ZyOS::QWORD usr_stack_save; // This is used by SysEntry.asm. If you mess with the offsets
        // make sure to adjust sysentry too.
        
        ZyOS::QWORD quantum;

        ZyOS::WORD current_core;
        ZyOS::WORD lowest_queue;
        ZyOS::WORD highest_queue;
        alignas(16) uint8_t *fx_state;
        uint8_t *malignedfx;
        bool running;
        bool core_pinned{};

        void block(BlockReasons reason, ZyOS::QWORD arg = 0);
        void unblock(BlockReasons unreason);
        void suicide();
        ZyOS::QWORD get_pid();
        void fork();
        void enqueue();
        void dequeue();
        int64_t compare(const lib::RB_Base* other) const override;

        static void TerminateTask(Task *term);
        static void UnblockAll(BlockReasons whoisblocking);
    private:
        ZyOS::WORD current_queue;
        ZyOS::QWORD pid;
        bool blockmap[(size_t)BlockReasons::TOTAL_REASONS]{false};
        void *_arg;
        static ZyOS::QWORD global_min_vruntime;
    };

    void EnableScheduler();
    void DisabledScheduler();
    void Initialize();
    void Yield();
    void Suicide();

    constexpr ZyOS::QWORD TASK_TABLE_SIZE = 1024;
    constexpr ZyOS::DWORD TASK_DIR_SIZE = 32;
    constexpr ZyOS::DWORD FX_STATE_SIZE = 0x200 + 0x10;
    extern Task ***TaskDirectory;
    extern ZyOS::QWORD watch_pid;

    Task *GetTaskByPID(ZyOS::QWORD PID);
    void RegisterSystemIdleTask(Task *task);
    Task *StealCoCoreTask();

    constexpr uint8_t TASK_STACK_PAGES = 8; // 8 * 4096 = 32KB of ram. Plenty.
    constexpr uint8_t TOTAL_SCHD_QUEUES = 32;
    constexpr uint8_t DEFAULT_SCHD_QUEUE = 0;
}