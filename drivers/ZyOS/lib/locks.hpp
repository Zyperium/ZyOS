#pragma once
#include <stdint.h>

namespace lib {
    class Spinlock {
    private:
        bool locked;
        uint64_t rflags;
    
    public:
        Spinlock() = default;

        void lock();
        void unlock();
    };

    class ScopedLock {
    private:
        Spinlock &lock;

    public:
        ScopedLock(Spinlock &lock);
        ~ScopedLock();
    };
}