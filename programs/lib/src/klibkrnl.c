#include <klibkrnl.h>
#include <stdint.h>
#include <stddef.h>
#include <ksyscall.h>
#include <string.h>

size_t kopen(const char *path) {
    size_t sz = strlen(path);
    return syscall(0, (uint64_t)path, sz);
}

size_t ksize(size_t file_d) {
    return syscall(1, file_d);
}

size_t kread(size_t file_d, size_t offset, uint8_t *buf, size_t bufsz) {
    return syscall(2, file_d, offset, bufsz, buf);
}

size_t kwrite(size_t file_d, size_t offset, uint8_t *buf, size_t bufsz) {
    return syscall(3, file_d, offset, bufsz, buf);
}

void kclose(size_t file_d) {
    syscall(4, file_d);
}

size_t ioctl(const char *drvr, size_t data, size_t ex) {
    size_t sz = strlen(drvr);
    return syscall(5, drvr, data, sz, ex);
}

void klog(const char *fmt, ...) {
    char buf[512];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        syscall(6, (uint64_t)buf, (uint64_t)len);
    }
}

void yield() {
    syscall(11);
}

size_t get_pid() {
    return syscall(20);
}

size_t fork() {
    return syscall(12);
}

size_t time() {
    return syscall(19);
}