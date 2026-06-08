#include <HAL/MEM/FMEM.hpp>

namespace FMEM {
    __attribute__((target("sse")))
    void FastFill32(uint32_t* dest, uint32_t color, size_t count) {
        if (count < 8) {
            while (count--) *dest++ = color;
            return;
        }

        while (((uintptr_t)dest & 0xF) && count > 0) {
            *dest++ = color;
            count--;
        }

        size_t chunks = count / 16;

        asm volatile(
            "movd %0, %%xmm0 \n"
            "pshufd $0, %%xmm0, %%xmm0 \n"
            
            "1: \n"
            "movdqa %%xmm0, 0(%1) \n" 
            "movdqa %%xmm0, 16(%1) \n"
            "movdqa %%xmm0, 32(%1) \n"
            "movdqa %%xmm0, 48(%1) \n"

            "add $64, %1 \n"
            "dec %2 \n"
            "jnz 1b \n"

            : /* no outputs */
            : "r"(color), "r"(dest), "r"(chunks)
            : "xmm0", "memory", "cc"
        );

        dest += chunks * 16;
        count %= 16;

        while (count--) {
            *dest++ = color;
        }
    }

    __attribute__((target("sse")))
    void FastFill8(uint8_t* dest, uint8_t val, size_t count) {
        uint32_t v32 = (uint32_t)val * 0x01010101;

        while (((uintptr_t)dest & 0xF) && count > 0) {
            *dest++ = val;
            count--;
        }

        size_t chunks = count / 64;

        if (chunks > 0) {
            asm volatile (
                "movd %0, %%xmm0 \n"
                "pshufd $0, %%xmm0, %%xmm0 \n"

                "1: \n"
                "movdqa %%xmm0, 0(%1) \n"
                "movdqa %%xmm0, 16(%1) \n"
                "movdqa %%xmm0, 32(%1) \n"
                "movdqa %%xmm0, 48(%1) \n"

                "add $64, %1 \n"
                "dec %2 \n"
                "jnz 1b \n"

                : /* No output variables */
                : "r"(v32), "r"(dest), "r"(chunks)
                : "xmm0", "memory", "cc"
            );

            dest += chunks * 64;
            count %= 64;
        }

        while (count--) {
            *dest++ = val;
        }
    }

    static bool are_interrupts_enabled() {
        uint64_t rflags;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        return (rflags & 0x200);
    }

    __attribute__((target("sse")))
    void FastCopy(void* d, const void* s, size_t count) {
        bool intre = are_interrupts_enabled();
        asm volatile("cli");
        uint8_t* dest = (uint8_t*)d;
        const uint8_t* src = (const uint8_t*)s;

        size_t chunks = count / 16;

        while (chunks--) {
            asm volatile(
                "movdqu (%1), %%xmm0\n"
                "movdqu %%xmm0, (%0)"
                :: "r"(dest), "r"(src)
                : "xmm0", "memory"
            );
            dest += 16;
            src += 16;
        }

        count %= 16;
        while (count--) {
            *dest++ = *src++;
        }

        if (intre)
            asm volatile("sti");
    }

    __attribute__((target("sse")))
    void enable_sse() {
        uint64_t cr0;
        uint64_t cr4;

        asm volatile("mov %%cr0, %0" : "=r"(cr0));
        cr0 &= ~(1 << 2);  // Clear EM (Emulation)
        cr0 |= (1 << 1);   // Set MP (Monitor Coprocessor)
        asm volatile("mov %0, %%cr0" :: "r"(cr0));

        asm volatile("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= (1 << 9);   // Set OSFXSR (FXSAVE/FXRSTOR)
        cr4 |= (1 << 10);  // Set OSXMMEXCPT (SIMD exceptions)
        asm volatile("mov %0, %%cr4" :: "r"(cr4));
    }
}
