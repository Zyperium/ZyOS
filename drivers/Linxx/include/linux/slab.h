#pragma once
#include <stddef.h>
#include <stdint.h>
#include <linux/gfp.h>

#ifdef __cplusplus
extern "C" {
#endif 
void *kmalloc(size_t size, gfp_t flags);
void kfree(void *ptr);
size_t ksize(void *ptr);
void *krealloc(const void *ptr, size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
#ifdef __cplusplus
}
#endif