#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void memset32(void *ptr, uint32_t value, size_t count);
int abs(int a);
void *memset(void *ptr, int value, size_t num);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t count);

uint64_t align_up(uint64_t adder, uint64_t alignment);
uint64_t align_down(uint64_t adder, uint64_t alignment);

bool strcmp(const char *strA, const char *strB);
bool strncmp(const char *strA, const char *strB, size_t max);
void strcpy(char *dest, const char *src);
void strncpy(char *dest, const char *src, size_t __max_length);
void strcat(char *dest, const char *src);
int strlen(const char *str);

void stoi(int_least64_t n, char *buffer);

#ifdef __cplusplus
}
#endif
