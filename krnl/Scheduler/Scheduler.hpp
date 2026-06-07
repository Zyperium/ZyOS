#pragma once

namespace Scheduler {
    enum class BlockReasons {
        SLEEP,
        AWAIT_FILE_IO,
        AWAIT_MOUSE_CURSOR,
        AWAIT_KEYBOARD_INPUT
    };

    struct TaskBlock {
        BlockReasons reason;
        TaskBlock *next;
    };

    class Task {
    public:
        Task();
        
        Task *next;
        Task *previous;
    private:
        TaskBlock *block_while;
    };

    void EnableScheduler();
    void DisabledScheduler();
}