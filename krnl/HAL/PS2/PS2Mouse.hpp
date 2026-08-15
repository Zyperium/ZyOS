#pragma once

#include <stdint.h>
#include <Services/Input/Input.hpp>

namespace PS2 {
    class Mouse {
    public:
        static constexpr uint16_t DATA_PORT    = 0x60;
        static constexpr uint16_t STATUS_PORT  = 0x64;
        static constexpr uint16_t COMMAND_PORT = 0x64;

        static constexpr uint8_t STATUS_OUTPUT_BUFFER_FULL = 0x01;
        static constexpr uint8_t STATUS_INPUT_BUFFER_FULL  = 0x02;

        static constexpr uint8_t CMD_READ_CONFIG_BYTE      = 0x20;
        static constexpr uint8_t CMD_WRITE_CONFIG_BYTE     = 0x60;
        static constexpr uint8_t CMD_ENABLE_SECOND_PORT    = 0xA8;
        static constexpr uint8_t CMD_WRITE_SECOND_PORT_IN  = 0xD4;

        static constexpr uint8_t CONFIG_PORT2_INTERRUPT    = (1 << 1);

        static constexpr uint8_t MOUSE_CMD_RESET           = 0xFF;
        static constexpr uint8_t MOUSE_CMD_SET_DEFAULTS    = 0xF6;
        static constexpr uint8_t MOUSE_CMD_ENABLE_STREAM   = 0xF4;

        static constexpr uint8_t MOUSE_RESP_ACK            = 0xFA;
        static constexpr uint8_t MOUSE_RESP_BAT_SUCCESS    = 0xAA;

        static constexpr uint8_t PACKET_BTN_MASK           = 0x07; // Bits 0-2 (Left, Right, Middle)
        static constexpr uint8_t PACKET_ALWAYS_ONE         = (1 << 3);
        static constexpr uint8_t PACKET_X_SIGN             = (1 << 4);
        static constexpr uint8_t PACKET_Y_SIGN             = (1 << 5);
        static constexpr uint8_t PACKET_X_OVERFLOW         = (1 << 6);
        static constexpr uint8_t PACKET_Y_OVERFLOW         = (1 << 7);

        static void Initialize();
        static void HandleInterrupt();

    private:
        static void WaitWrite();
        static void WaitRead();
        static void WriteCommand(uint8_t cmd);
        static uint8_t ReadData();

        static inline uint8_t packet_buffer[3]{};
        static inline uint8_t packet_cycle{0};
    };
}