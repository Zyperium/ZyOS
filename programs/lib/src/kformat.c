#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

static void reverse(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static int itoa_dec(int64_t num, char *str) {
    int i = 0;
    int is_negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse(str, i);
    return i;
}

static int itoa_hex(uint64_t num, char *str, int uppercase) {
    int i = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }

    while (num != 0) {
        int rem = num % 16;
        str[i++] = digits[rem];
        num = num / 16;
    }

    str[i] = '\0';
    reverse(str, i);
    return i;
}

int vsnprintf(char *buffer, size_t count, const char *format, va_list va) {
    if (!buffer || count == 0) return 0;

    size_t written = 0;

    while (*format && written < count - 1) {
        if (*format != '%') {
            buffer[written++] = *format++;
            continue;
        }

        format++;
        
        int is_sizet = 0;
        if (*format == 'z') {
            is_sizet = 1;
            format++;
        }

        switch (*format) {
            case 's': {
                const char *s = va_arg(va, const char *);
                if (!s) s = "(null)";
                while (*s && written < count - 1) {
                    buffer[written++] = *s++;
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(va, int);
                buffer[written++] = c;
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_sizet ? (int64_t)va_arg(va, size_t) : (int64_t)va_arg(va, int);
                char tmp[32];
                itoa_dec(val, tmp);
                for (int i = 0; tmp[i] && written < count - 1; i++) {
                    buffer[written++] = tmp[i];
                }
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val = is_sizet ? (uint64_t)va_arg(va, size_t) : (uint64_t)va_arg(va, unsigned int);
                char tmp[32];
                itoa_hex(val, tmp, (*format == 'X'));
                for (int i = 0; tmp[i] && written < count - 1; i++) {
                    buffer[written++] = tmp[i];
                }
                break;
            }
            case 'p': {
                uint64_t ptr = (uint64_t)va_arg(va, void *);
                if (written < count - 3) {
                    buffer[written++] = '0';
                    buffer[written++] = 'x';
                }
                char tmp[32];
                itoa_hex(ptr, tmp, 0);
                for (int i = 0; tmp[i] && written < count - 1; i++) {
                    buffer[written++] = tmp[i];
                }
                break;
            }
            case '%': {
                buffer[written++] = '%';
                break;
            }
            case 'u': {
                uint64_t val = is_sizet ? (uint64_t)va_arg(va, size_t) : (uint64_t)va_arg(va, unsigned int);
                char tmp[32];
                itoa_dec(val, tmp);
                for (int i = 0; tmp[i] && written < count - 1; i++) {
                    buffer[written++] = tmp[i];
                }
                break;
            }
            default: {
                buffer[written++] = *format;
                break;
            }
        }
        format++;
    }

    buffer[written] = '\0';
    return (int)written;
}