#include <Library/locks.hpp>
#include <Library/regs.h>

namespace lib {
    void Spinlock::lock() {
        uint64_t _flags;
        asm volatile("pushfq; pop %0" : "=r"(_flags));
        asm volatile("cli");

        while (__atomic_test_and_set(&locked, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        rflags = _flags;
    }

    void Spinlock::unlock() {
        // Why? Because otherwise there could be a preemption during a time where rflags are unlocked
        // but the lock is active. That would deadlock.
        uint64_t _flags = rflags;
        rflags = 0;
        __atomic_clear(&locked, __ATOMIC_RELEASE);
        restore_rflags(_flags);

        return;
    }

    ScopedLock::ScopedLock(Spinlock &plock) : lock(plock) {
        lock.lock();
    }

    ScopedLock::~ScopedLock() {
        lock.unlock();
    }
}