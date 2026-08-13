#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int vsnprintf(char *buffer, size_t count, const char *format, va_list va);
size_t kopen(const char *path);
size_t ksize(size_t file_d);
size_t kread(size_t file_d, size_t offset, uint8_t *buf, size_t bufsz);
size_t kwrite(size_t file_d, size_t offset, uint8_t *buf, size_t bufsz);
void kclose(size_t file_d);

size_t ioctl(const char *drvr, size_t data, size_t ex);

void yield();
void klog(const char *fmt, ...);
size_t get_pid();
size_t fork();
size_t time();

#ifdef __cplusplus
}
#endif