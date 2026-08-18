#include <Library/debug.hpp>
#include <Library/locks.hpp>
#include <Library/regs.h>

namespace lib {
    constexpr uint64_t RFLAGS_IF = (1ULL << 9);

    __attribute__((no_stack_protector))
    uint64_t Spinlock::lock() {
        uint64_t _flags;
        asm volatile(
            "pushfq\n\t"
            "pop %0\n\t"
            "cli"
            : "=r"(_flags)
            :
            : "memory"
        );

        while (__atomic_test_and_set(&locked, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
            // Debug::krnl_print("SCHD", Debug::LOG_INFO, "Stuck!");
        }
        asm volatile("sfence");
        return _flags;
    }

    __attribute__((no_stack_protector))
    void Spinlock::unlock(uint64_t _flags) {
        __atomic_clear(&locked, __ATOMIC_RELEASE);
        asm volatile("sfence");
        if (_flags & RFLAGS_IF)
            asm volatile("sti");
    }

    __attribute__((no_stack_protector))
    void SoftLock::unlock() {
        __atomic_clear(&locked, __ATOMIC_RELEASE);
    }

    __attribute__((no_stack_protector))
    void SoftLock::lock() {
        while (__atomic_test_and_set(&locked, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
            // Debug::krnl_print("SCHD", Debug::LOG_INFO, "Stuck!");
        }
    }

    ScopedLock::ScopedLock(Spinlock &plock) : lock(plock) {
        rflags = lock.lock();
    }

    ScopedLock::~ScopedLock() {
        lock.unlock(rflags);
    }
}