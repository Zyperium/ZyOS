#pragma once
#include <stdint.h>

inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void insw(uint16_t port, void* addr, uint32_t count) {
    asm volatile("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outw(uint16_t port, uint16_t val) {
   asm volatile( "outw %0, %1" : : "a"(val), "Nd"(port) );
}

static inline void wait() {
    outb(0x80, 0);
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}