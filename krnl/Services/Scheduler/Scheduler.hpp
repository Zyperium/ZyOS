#pragma once
#include "Library/string.h"
#include <Library/umap.hpp>
#include <Library/ZyOS.hpp>
#include <Library/cystr.hpp>
#include <Library/redblack.hpp>
#include <Library/locks.hpp>
#include <Library/vec.hpp>

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
        FORK,
        FORKER,
        FORKD,
        TOTAL_REASONS
    };

    class Task;

    constexpr uint64_t TOTAL_QUEUES = PAGE_SIZE / sizeof(Task *) - 1;
    struct GarbageQueue {
        uint32_t curr_ptr;
        Task *to_clear[TOTAL_QUEUES];
    };

    constexpr uint8_t MAX_USR_FD = 16;
    constexpr size_t USR_MMAP_BEGIN = 0x0000'7000'0000'0000;
    struct UserTask {
        uint64_t rip;
        VFS::VNode *descriptors[MAX_USR_FD]{nullptr};
        Task *task_owner{nullptr};
        size_t permissions{0};
        size_t next_free_ds{0};
        size_t usr_virt_mmap{USR_MMAP_BEGIN};
        lib::vec<lib::string> opened_drvrs;

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
        UserTask *utask; // Accessed by SysEntry.asm

        uint8_t *fx_state;
        ZyOS::QWORD last_ran_time;
        ZyOS::DWORD niceness;
        ZyOS::QWORD used_ram;
        ZyOS::WORD current_core;
        volatile bool running;
        volatile bool is_queued;
        bool core_pinned;
        bool syscalls_allowed;

        void block(BlockReasons reason, ZyOS::QWORD arg = 0);
        void unblock(BlockReasons unreason);
        void suicide();
        void sleep(ZyOS::QWORD time);
        void wake();
        void fork();
        void enqueue();
        void dequeue();
        int64_t compare(const lib::RB_Base *other) const override;
        ZyOS::QWORD get_wake_time();
        ZyOS::QWORD get_pid();

        static void TerminateTask(Task *term);
        static void UnblockAll(BlockReasons whoisblocking);
        static Task *GetNextTask();
    private:
        ZyOS::QWORD pid;
        BlockReasons blocked;
        uint64_t blocked_by;
        void *_arg;
        static ZyOS::QWORD global_min_vruntime;
        static lib::Spinlock lock;
    };

    struct NotifData { 
        uint32_t sender_id;
        uint32_t data_id;
        uint64_t contents;
    };

    void EnableScheduler();
    void WakeAndSendNotif(ZyOS::QWORD process_id, const NotifData &data);
    void DisabledScheduler();
    void ClearGarbage();
    void ForkerTask();
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
    extern Task *frkr_task;
    extern Task *reaper_task;

    Task *GetTaskByPID(ZyOS::QWORD PID);
    void RegisterSystemIdleTask(Task *task);
    Task *StealCoCoreTask();

    constexpr uint8_t TASK_STACK_PAGES = 4; // stingy? yes.
    constexpr uint8_t TOTAL_SCHD_QUEUES = 32;
    constexpr uint16_t MIN_VRUNTIME_OFFSET = 128;
    constexpr uint8_t DEFAULT_SCHD_QUEUE = 0;

    #define FORK_APPEND_TEXT " (Forked)"
}
