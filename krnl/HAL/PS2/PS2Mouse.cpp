#include <HAL/PS2/PS2Mouse.hpp>
#include <HAL/CORE/Core.hpp>
#include <Library/io.hpp>
#include <Library/debug.hpp>
#include <Services/Input/Input.hpp>

extern "C" void MSHI_Wrapper() {
    PS2::Mouse::HandleInterrupt();
    HAL::CORE::ack_lapic();
    return;
}

namespace PS2 {
    void Mouse::WaitWrite() {
        while (inb(STATUS_PORT) & STATUS_INPUT_BUFFER_FULL) {
            asm volatile("pause");
        }
    }

    void Mouse::WaitRead() {
        while (!(inb(STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL)) {
            asm volatile("pause");
        }
    }

    void Mouse::WriteCommand(uint8_t cmd) {
        WaitWrite();
        outb(COMMAND_PORT, CMD_WRITE_SECOND_PORT_IN);
        WaitWrite();
        outb(DATA_PORT, cmd);
    }

    uint8_t Mouse::ReadData() {
        WaitRead();
        return inb(DATA_PORT);
    }

    void Mouse::Initialize() {
        Debug::krnl_print("PS/2 Mouse", Debug::LOG_INFO, "Initializing Mouse...");

        WaitWrite();
        outb(COMMAND_PORT, CMD_ENABLE_SECOND_PORT);

        WaitWrite();
        outb(COMMAND_PORT, CMD_READ_CONFIG_BYTE);
        uint8_t config = ReadData();

        config |= CONFIG_PORT2_INTERRUPT;
        config &= ~(1 << 5);

        WaitWrite();
        outb(COMMAND_PORT, CMD_WRITE_CONFIG_BYTE);
        WaitWrite();
        outb(DATA_PORT, config);

        WriteCommand(MOUSE_CMD_RESET);
        if (ReadData() != MOUSE_RESP_ACK) {
            Debug::krnl_print("PS/2 Mouse", Debug::LOG_ERROR, "Reset ACK failed");
            return;
        }
        if (ReadData() != MOUSE_RESP_BAT_SUCCESS || ReadData() != 0x00) {
            Debug::krnl_print("PS/2 Mouse", Debug::LOG_ERROR, "Self-test failed");
            return;
        }

        WriteCommand(MOUSE_CMD_SET_DEFAULTS);
        if (ReadData() != MOUSE_RESP_ACK) {
            Debug::krnl_print("PS/2 Mouse", Debug::LOG_ERROR, "Set defaults ACK failed");
            return;
        }

        WriteCommand(MOUSE_CMD_ENABLE_STREAM);
        if (ReadData() != MOUSE_RESP_ACK) {
            Debug::krnl_print("PS/2 Mouse", Debug::LOG_ERROR, "Enable streaming ACK failed");
            return;
        }

        Debug::krnl_print("PS/2 Mouse", Debug::LOG_INFO, "Mouse fully initialized");
    }

    void Mouse::HandleInterrupt() {
        if (!(inb(STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL)) {
            return;
        }

        uint8_t byte = inb(DATA_PORT);

        if (packet_cycle == 0 && !(byte & PACKET_ALWAYS_ONE)) {
            return;
        }

        packet_buffer[packet_cycle++] = byte;

        if (packet_cycle == 3) {
            packet_cycle = 0;

            uint8_t flags = packet_buffer[0];

            if (flags & (PACKET_X_OVERFLOW | PACKET_Y_OVERFLOW)) {
                return;
            }

            int8_t rel_x = static_cast<int8_t>(packet_buffer[1]);
            int8_t rel_y = static_cast<int8_t>(packet_buffer[2]);

            Input::MousePos pos{
                .delta_x = rel_x,
                .delta_y = static_cast<int8_t>(-rel_y),
                .buttons = static_cast<uint8_t>(flags & PACKET_BTN_MASK),
                .scroll_wheel = 0
            };

            Input::add_mouse(pos);
        }
    }
}