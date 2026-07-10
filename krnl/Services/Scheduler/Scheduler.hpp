#pragma once
#include <Library/ZyOS.hpp>
#include <Library/cystr.hpp>
#include <Library/redblack.hpp>

#include <HAL/ACPI/ACPI.hpp>

#include <Services/VFS/VFS.hpp>

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

    constexpr uint8_t MAX_USR_FD = 16;
    struct UserTask {
        VFS::VNode *descriptors[MAX_USR_FD]{nullptr};
        size_t permissions{0};
        size_t next_free_ds{0};

        ~UserTask();
        UserTask() = default;
    };

    /*
        WARNING: This class is used in assembly. You must match any changes with the assembly
    */
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
        ZyOS::QWORD *krnl_stack_top; // Saved by SysEntry.asm
        ZyOS::QWORD *krnl_stack_btm;
        ZyOS::QWORD usr_stack_save; /* This is used by SysEntry.asm. If you mess with the offsets
        make sure to adjust sysentry too. */
        UserTask *utask;

        alignas(16) uint8_t *fx_state;
        uint8_t *malignedfx;
        ZyOS::QWORD last_ran_time;
        ZyOS::DWORD niceness;
        ZyOS::WORD current_core;
        volatile bool running;
        bool core_pinned;
        bool syscalls_allowed;

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
        static Task *GetNextTask();
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
    inline uint64_t LastRunTime(Task *task) {
        return ACPI::get_sys_time() - task->last_ran_time;
    }

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
    constexpr uint16_t MIN_VRUNTIME_OFFSET = 128;
    constexpr uint8_t DEFAULT_SCHD_QUEUE = 0;
}