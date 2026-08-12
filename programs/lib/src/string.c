#include "string.h"

void memset32(void *ptr, uint32_t value, size_t count) {
    asm volatile (
        "cld;"
        "rep stosl;"
        : "+D"(ptr), "+c"(count)
        : "a"(value)
        : "memory"
    );
}

int abs(int a) {
    int out;
    __asm__ (
        "movl %1, %%eax\n\t"
        "cltd\n\t"
        "xorl %%edx, %%eax\n\t"
        "subl %%edx, %%eax\n\t"
        "movl %%eax, %0"
        : "=r" (out)
        : "r" (a)
        : "eax", "edx"
    );
    return out;
}

void *memset(void *ptr, int value, size_t num) {
    unsigned char *p = (unsigned char *)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pDest = (uint8_t *)dest;
    const uint8_t *pSrc = (const uint8_t *)src;

    while (n >= 8) {
        *(uint64_t *)pDest = *(const uint64_t *)pSrc;
        pDest += 8;
        pSrc += 8;
        n -= 8;
    }

    if (n >= 4) {
        *(uint32_t *)pDest = *(const uint32_t *)pSrc;
        pDest += 4;
        pSrc += 4;
        n -= 4;
    }

    while (n > 0) {
        *pDest++ = *pSrc++;
        n--;
    }

    return dest;
}

uint64_t align_up(uint64_t adder, uint64_t alignment) {
    return (adder + alignment - 1) & ~(alignment - 1);
}

uint64_t align_down(uint64_t adder, uint64_t alignment) {
    return adder & ~(alignment - 1);
}

void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || count == 0) {
        return dest;
    }

    if (d < s) {
        for (size_t i = 0; i < count; ++i) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = count; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

bool strcmp(const char *strA, const char *strB) {
    while (*strA == *strB) {
        if (*strA == '\0')
            return true;

        strA++;
        strB++;
    }
    return false;
}

void strcpy(char *dest, const char *src) {
    if (!dest || !src) return;

    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

void strncpy(char *dest, const char *src, size_t __max_length) {
    if (!dest || !src) return;

    if (!__max_length) return;

    while (*src != '\0' && __max_length > 0) {
        *dest = *src;
        --__max_length;
        ++dest;
        ++src;
    }
    *dest = '\0';
}

bool strncmp(const char *strA, const char *strB, size_t max) {
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

void strcat(char *dest, const char *src) {
    if (dest == NULL || src == NULL) return;

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

int strlen(const char *str) {
    int count = 0;
    while (*str != '\0') {
        ++str;
        ++count;
    }
    return count;
}

void stoi(int_least64_t n, char *buffer) {
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