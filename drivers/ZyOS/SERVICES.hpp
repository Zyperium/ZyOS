#pragma once
#include <stdint.h>
#include <lib/str.hpp>
#include <lib/redblack.hpp>
#include <lib/umap.hpp>
#include <lib/vec.hpp>

namespace ELF {
    /*
        @brief How do I use this?
        Well, unless you want your driver task to be a ring 3 app, which I assume you don't,
        you need to spawn a *new* task with the runway as its entry point, pass then cmd_line
        through the void *argument, and then chill! Not too hard at all.
    */
    void Runway(lib::string cmd_line);

    /*
        @note Probably don't use this, unless you need some hyper specific function.
        (Basically you want the elf in memory & patched, but not executing)
    */
    void *load_elf(lib::string path);
}

namespace VFS {
    enum class FileType {
        Regular,
        Directory,
        CharDevice
    };

    class VNode {
    protected:
        FileType m_type;
        uint64_t m_size;
        uint32_t m_ref_count{1};

    public:
        VNode(FileType type, uint64_t size);
        virtual ~VNode() = default;
        
        virtual int read(uint64_t offset, void* buffer, uint32_t size) = 0;
        virtual int write(uint64_t offset, const void* buffer, uint32_t size) = 0;
        virtual VNode* create(const char* name, FileType type) = 0;
        virtual VNode* lookup(const char* name) = 0;

        FileType get_type() const;
        uint64_t get_size() const;
        
        void add_ref();
        void release();

        VFS::VNode* resolve_path_to_vnode(const lib::string& path);
    };

    struct FileHandle {
        VNode* vnode {nullptr};
        uint64_t current_offset {0};
        uint32_t flags {0};
        bool valid {false};
    };
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

    constexpr uint8_t MAX_USR_FD = 16;
    constexpr size_t USR_MMAP_BEGIN = 0x0000'7000'0000'0000;
    struct UserTask {
        VFS::VNode *descriptors[MAX_USR_FD]{nullptr};
        size_t permissions{0};
        size_t next_free_ds{0};
        size_t usr_virt_mmap{USR_MMAP_BEGIN};
        lib::vec<lib::string> opened_drvrs;

        ~UserTask();
        UserTask() = default;
    };

    class alignas(64) Task : public lib::RB_Base {
    public:
        using EntryPoint = void(*)(void*);
        Task();
        Task(EntryPoint entry, lib::string name, bool enqueue = true, void *p_arg = nullptr);

        lib::string task_name;

        uint64_t rsp;
        uint64_t heap_ptr;
        uint64_t mapped_limit;
        uint64_t cr3;
        uint64_t fs_base;
        uint64_t vruntime;

        uint64_t *usr_stack_top;
        uint64_t *krnl_stack_top; // Saved by SysEntry.asm
        uint64_t *krnl_stack_btm;
        uint64_t usr_stack_save; /* This is used by SysEntry.asm. If you mess with the offsets
        make sure to adjust sysentry too. */
        UserTask *utask;

        alignas(16) uint8_t *fx_state;
        uint8_t *malignedfx;
        uint64_t last_ran_time;
        uint32_t niceness;
        uint16_t current_core;
        volatile bool running;
        bool core_pinned;
        bool syscalls_allowed;

        void block(BlockReasons reason, uint64_t arg = 0);
        void unblock(BlockReasons unreason);
        void suicide();
        void sleep(uint64_t time);
        void wake();
        void fork();
        void enqueue();
        void dequeue();
        int64_t compare(const lib::RB_Base *other) const override;
        uint64_t get_wake_time();
        uint64_t get_pid();

        static void TerminateTask(Task *term);
        static void UnblockAll(BlockReasons whoisblocking);
        static Task *GetNextTask();
    private:
        uint16_t current_queue;
        uint64_t pid;
        bool blockmap[(size_t)BlockReasons::TOTAL_REASONS]{false};
        void *_arg;
        static uint64_t global_min_vruntime;
        static lib::Spinlock lock;
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

namespace IPC {
    using DrvioCB = uint64_t (*)(Scheduler::Task *);
    using DrvioIR = uint64_t (*)(Scheduler::Task *, uint64_t details);

    class drvio {
    public:
        // Expose a path to your driver. E.G: "my_driver/" or "zyos/my_driver/"
        drvio(lib::string expose_dir);

        uint64_t add_relier(Scheduler::Task *new_reliee);
        void remove_relier(Scheduler::Task *unlinker);
        bool is_relier(Scheduler::Task *relier);

        DrvioCB on_entry;
        DrvioCB on_exit;
        DrvioIR on_call;

    private:
        lib::string exposee;
        lib::umap<uint64_t, lib::string> reliees;
    };

    extern lib::umap<lib::string, drvio *> regdrvrs;
}