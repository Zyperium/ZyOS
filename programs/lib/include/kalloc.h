#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t bytes);
void free(void *mem);
void *realloc(void *ptr, size_t new_size);
void *calloc(size_t num, size_t size);

#ifdef __cplusplus
}
#endif