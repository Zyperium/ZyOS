#include <stdarg.h>
#include <stddef.h>

#include <Library/debug.hpp>
#include <Library/io.hpp>
#include <Library/regs.h>
#include <HAL/CORE/Core.hpp>

namespace Debug {
    constexpr uint8_t nice_col[] = {
        31, 32, 33, 34, 35, 36,
        91, 92, 93, 94, 95, 96, 97
    };

    bool a_debug_lock = false;
    uint64_t cur_rflags = 0;
    void aquire_lock() {
        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        asm volatile("cli");
        while (__atomic_test_and_set(&a_debug_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        cur_rflags = rflags;

        return;
    }

    void release_lock() {
        __atomic_clear(&a_debug_lock, __ATOMIC_RELEASE);
        restore_rflags(cur_rflags);
        cur_rflags = 0;
        return;
    }

    uint8_t str_to_col(const char* str) { 
        uint32_t hash = 1237389;
        while (*str) 
            hash = ((hash << 5) + hash) + *str++;
        return nice_col[hash % (sizeof(nice_col) / sizeof(nice_col[0]))]; 
    }

    constexpr uint8_t level_color(LogLevel lvl) {
        switch (lvl) {
            case LOG_INFO:  return 34; // blue
            case LOG_WARN:  return 33; // yellow
            case LOG_ERROR: return 31; // red
        }
        return 37;
    }

    constexpr const char* level_name(LogLevel lvl) {
        switch (lvl) {
            case LOG_INFO:  return "INFO";
            case LOG_WARN:  return "WARN";
            case LOG_ERROR: return "ERROR";
        }
        return "?";
    }

    inline void putc(char c) {
        outb(0xE9, c);
    }

    void puts(const char* s) {
        while (*s) putc(*s++);
    }

    void print_int(int64_t value) {
        if (value < 0) {
            putc('-');
            value = -value;
        }

        char buf[20];
        int i = 0;

        do {
            buf[i++] = '0' + (value % 10);
            value /= 10;
        } while (value);

        while (i--) putc(buf[i]);
    }

    static void buf_puts(char *&buf, size_t &remaining, const char* s) {
        while (*s && remaining > 1) {
            *buf++ = *s++;
            remaining--;
        }
    }

    int vsnprintf(char* buffer, size_t n, const char* fmt, va_list args) {
        if (n == 0) return 0;
        
        char* ptr = buffer;
        size_t remaining = n;

        for (; *fmt && remaining > 1; fmt++) {
            if (*fmt != '%') {
                *ptr++ = *fmt;
                remaining--;
                continue;
            }

            fmt++;
            switch (*fmt) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    buf_puts(ptr, remaining, s ? s : "(null)");
                    break;
                }
                case 'd':
                case 'i': {
                    int64_t val = va_arg(args, int);
                    char tmp[20];
                    int i = 0;
                    if (val < 0) { if(remaining > 1) { *ptr++ = '-'; remaining--; } val = -val; }
                    do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val);
                    while (i-- && remaining > 1) { *ptr++ = tmp[i]; remaining--; }
                    break;
                }
                case 'x': {
                    uint64_t val = va_arg(args, uint64_t);
                    const char* hex = "0123456789ABCDEF";
                    if (remaining > 2) { *ptr++ = '0'; *ptr++ = 'x'; remaining -= 2; }
                    for (int i = 60; i >= 0; i -= 4) {
                        if (remaining > 1) { *ptr++ = hex[(val >> i) & 0xF]; remaining--; }
                    }
                    break;
                }
                case '%': {
                    *ptr++ = '%';
                    remaining--;
                    break;
                }
                default: {
                    *ptr++ = '?';
                    remaining--;
                    break;
                }
            }
        }

        *ptr = '\0';
        return ptr - buffer;
    }

    int snprintf(char* buffer, size_t n, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int result = vsnprintf(buffer, n, fmt, args);
        va_end(args);
        return result;
    }

    void print_hex(uint64_t value) {
        puts("0x");
        const char* hex = "0123456789ABCDEF";

        for (int i = 60; i >= 0; i -= 4)
            putc(hex[(value >> i) & 0xF]);
    }

    __attribute__((target("sse,sse2")))
    void print_float(double value, int precision = 2) {
        if (value < 0) {
            putc('-');
            value = -value;
        }

        int64_t whole = (int64_t)value;
        print_int(whole);

        putc('.');

        double frac = value - whole;
        for (int i = 0; i < precision; i++) {
            frac *= 10;
            int digit = (int)frac;
            putc('0' + digit);
            frac -= digit;
        }
    }

    __attribute__((target("sse,sse2")))
    void kvprintf(const char* fmt, va_list args) {
        for (; *fmt; fmt++) {
            if (*fmt != '%') {
                putc(*fmt);
                continue;
            }

            fmt++;
            switch (*fmt) {
                case 'd':
                case 'i':
                    print_int(va_arg(args, int));
                    break;

                case 'u':
                    print_int(va_arg(args, unsigned));
                    break;

                case 'x':
                    print_hex(va_arg(args, uint64_t));
                    break;

                case 'f':
                    print_float(va_arg(args, double));
                    break;

                case 's':
                    puts(va_arg(args, const char*));
                    break;

                case '%':
                    putc('%');
                    break;

                default:
                    putc('?');
                    break;
            }
        }
    }
    
    void krnl_print(const char* class_name,
               LogLevel level,
               const char* fmt, ...)
    {
        aquire_lock();
        puts("\033[");
        print_int(str_to_col(class_name));
        puts("m[");
        puts(class_name);
        puts("]\033[0m ");

        bool da = HAL::CORE::validate_gs_reg();
        if (da) {
            puts("[");
            print_int(HAL::CORE::get_thread_data()->core_id);
            puts("] ");
        }
        else {
            puts("[0] ");
        }

        puts("\033[");
        print_int(level_color(level));
        puts("m<");
        puts(level_name(level));
        puts(">\033[0m ");

        va_list args;
        va_start(args, fmt);
        kvprintf(fmt, args);
        va_end(args);

        putc('\n');
        release_lock();
    }
}