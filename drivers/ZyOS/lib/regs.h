#include <stdint.h>

inline void restore_rflags(uint64_t old_rflags) {
    __asm__ __volatile__(
        "push %0\n\t"
        "popfq"
        :
        : "g"(old_rflags)
        : "cc", "memory"
    );
}

[[nodiscard]] inline uint64_t read_cr3() {
    uint64_t cr3_val;
    asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
    return cr3_val;
}

constexpr uint64_t RFLAGS_IF_MASK (1ULL << 9);
static inline bool is_interrupt_enabled() {
    uint64_t rflags;
    
    __asm__ __volatile__(
        "pushfq\n\t"        // Push 64-bit RFLAGS onto stack
        "pop %0"            // Pop value into general register
        : "=r" (rflags)
        : /* no inputs */
        : "memory"          // Protect stack ordering
    );
    
    return (rflags & RFLAGS_IF_MASK) != 0;
}
