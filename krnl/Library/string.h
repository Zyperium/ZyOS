#pragma once
#include <stdint.h>
#include <stddef.h>

constexpr uint64_t PAGE_SIZE = 4096;

inline void memset32(void* ptr, uint32_t value, size_t count) {
    asm volatile (
        "cld;"
        "rep stosl;"
        : "+D"(ptr), "+c"(count)
        : "a"(value)
        : "memory"
    );
}

inline void memset(void *ptr, int value, size_t num) {
    unsigned char *p = (unsigned char*)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
}

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

inline uint64_t align_up(uint64_t adder, uint64_t alignment) {
    return (adder + alignment - 1) & ~(alignment - 1);
}

inline uint64_t align_down(uint64_t adder, uint64_t alignment) {
    return adder & ~(alignment - 1);
}

inline bool strcmp(const char *strA, const char *strB) {
    while (*strA == *strB) {
        if (*strA == '\0')
            return true;

        strA++;
        strB++;
    }
    return false;
}

inline void strcpy(char* dest, const char* src) {
    if (!dest || !src) return;
    
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

inline bool strncmp(const char *strA, const char *strB, size_t max) {
    size_t m_max = max;
    
    while (m_max && *strA == *strB) {
        if (*strA == '\0')
            return true;

        strA++;
        strB++;
        m_max--;
    }
    if (m_max == 0) return true;
    
    return false;
}

inline void strcat(char *dest, const char *src) {
    if (dest == nullptr || src == nullptr) return;
    
    while (*dest != '\0') {
        dest++;
    }

    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }

    *dest = '\0';
}

inline int strlen(const char *str) {
    int count = 0;
    while (*str != '\0') {
        ++str;
        ++count;
    }
    return count;
}

inline void stoi(int_least64_t n, char* buffer) {
    int i = 0;
    bool isNegative = false;

    if (n == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }

    if (n < 0) {
        isNegative = true;
        n = -n;
    }

    while (n != 0) {
        int rem = n % 10;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        n = n / 10;
    }

    if (isNegative) {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';
    int start = 0;
    int end = i - 1;

    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
}