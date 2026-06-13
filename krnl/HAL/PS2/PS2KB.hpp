#pragma once
#include <stdint.h>

namespace PS2 {
    class Keyboard {
    private:
        static constexpr uint16_t DATA_PORT = 0x60;
        static constexpr uint16_t STATUS_PORT = 0x64;

        static bool shift_pressed;
        static bool caps_lock;
        static bool extended_code;

        static char TranslateScancode(uint8_t scancode);
        static void WaitWrite();
        static void WaitRead();
        Keyboard() = default;
    public:
        static void HandleInterrupt();
        static void Initialize();
    };

    static const char scancode_set1_lowercase[] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
      '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
     '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
    };

    static const char scancode_set1_uppercase[] = {
        0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
      '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
     '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
    };
}