#pragma once
#include <stdint.h>

namespace lib {
    class Spinlock {
    private:
        bool locked;
        uint64_t rflags;
    
    public:
        Spinlock() = default;

        virtual uint64_t lock();
        virtual void unlock(uint64_t);
    };

    class SoftLock {
    private:
        bool locked;
        uint64_t rflags;
    
    public:
        SoftLock() = default;

        void lock();
        void unlock();
    };

    class ScopedLock {
    private:
        Spinlock lock;
        uint64_t rflags{};

    public:
        explicit ScopedLock(Spinlock &lock);
        ~ScopedLock();

        ScopedLock(const ScopedLock &) = delete;
        ScopedLock &operator=(const ScopedLock &) = delete;
    };

    class ScopedSoftLock {
    private:
        SoftLock lock;

    public:
        explicit ScopedSoftLock(SoftLock &lock);
        ~ScopedSoftLock();

        ScopedSoftLock(const ScopedSoftLock &) = delete;
        ScopedSoftLock &operator=(const ScopedSoftLock &) = delete;
    };
}