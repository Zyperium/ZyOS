#include <stddef.h>

// For some reason g++26 wants implicit calls to this.
// So this is here so the OS compiles. You probably
// still want to use the inline version in string.h

extern "C" {
    __attribute__((noinline)) 
    void* memset(void *ptr, int value, size_t num) {
        unsigned char *p = (unsigned char*)ptr;
        while (num--) {
            *p++ = (unsigned char)value;
        }
        return ptr;
    }
}