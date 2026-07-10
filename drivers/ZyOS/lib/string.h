#include <stdint.h>
#include <stddef.h>

inline void memcpy(void* dest, const void* src, size_t n) {
    uint8_t* pDest = (uint8_t*)dest;
    const uint8_t* pSrc = (const uint8_t*)src;

    while (n >= 8) {
        *(uint64_t*)pDest = *(const uint64_t*)pSrc;
        pDest += 8;
        pSrc += 8;
        n -= 8;
    }

    if (n >= 4) {
        *(uint32_t*)pDest = *(const uint32_t*)pSrc;
        pDest += 4;
        pSrc += 4;
        n -= 4;
    }

    while (n > 0) {
        *pDest++ = *pSrc++;
        n--;
    }
}