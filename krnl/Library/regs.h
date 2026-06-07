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