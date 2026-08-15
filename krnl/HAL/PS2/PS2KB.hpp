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
        static constexpr uint16_t COMMAND_PORT = 0x64;

        static constexpr uint8_t STATUS_OUTPUT_BUFFER_FULL = 0x01;
        static constexpr uint8_t STATUS_INPUT_BUFFER_FULL  = 0x02;

        static constexpr uint8_t CMD_DISABLE_FIRST_PORT   = 0xAD;
        static constexpr uint8_t CMD_DISABLE_SECOND_PORT  = 0xA7;
        static constexpr uint8_t CMD_READ_CONFIG_BYTE     = 0x20;
        static constexpr uint8_t CMD_WRITE_CONFIG_BYTE    = 0x60;
        static constexpr uint8_t CMD_ENABLE_FIRST_PORT    = 0xAE;

        static constexpr uint8_t CONFIG_PORT1_INTERRUPT    = (1 << 0);
        static constexpr uint8_t CONFIG_SYSTEM_POST_PASSED = (1 << 2);
        static constexpr uint8_t CONFIG_PORT1_CLOCK_ENABLE = (0 << 4);
        static constexpr uint8_t CONFIG_PORT1_TRANSLATION  = (1 << 6);

        static constexpr uint8_t DEV_CMD_ENABLE_SCANNING   = 0xF4;
        static constexpr uint8_t DEV_RESP_ACK              = 0xFA;

        static constexpr uint8_t SCANCODE_EXTENDED_PREFIX = 0xE0;
        static constexpr uint8_t SCANCODE_RELEASE_FLAG    = 0x80;
        static constexpr uint8_t SCANCODE_LEFT_SHIFT      = 0x2A;
        static constexpr uint8_t SCANCODE_RIGHT_SHIFT     = 0x36;
        static constexpr uint8_t SCANCODE_CAPS_LOCK       = 0x3A;
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