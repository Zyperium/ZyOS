#include <HAL/PS2/PS2KB.hpp>
#include <HAL/CORE/Core.hpp>
#include <Library/io.hpp>
#include <Library/debug.hpp>

#include <Services/Input/Input.hpp>

extern "C" void KBHI_Wrapper() {
    PS2::Keyboard::HandleInterrupt();
    HAL::CORE::ack_lapic();
    return;
}

namespace PS2 {
    bool Keyboard::shift_pressed{};
    bool Keyboard::caps_lock{};
    bool Keyboard::extended_code{};

    void Keyboard::HandleInterrupt() {
        if (!(inb(STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL)) {
            return; 
        }

        uint8_t scancode = inb(DATA_PORT);

        if (scancode == SCANCODE_EXTENDED_PREFIX) {
            extended_code = true;
            return;
        }

        if (scancode & SCANCODE_RELEASE_FLAG) {
            uint8_t make_code = scancode & ~SCANCODE_RELEASE_FLAG;
            
            if (make_code == SCANCODE_LEFT_SHIFT || make_code == SCANCODE_RIGHT_SHIFT) {
                shift_pressed = false;
            }

            extended_code = false;
            return;
        }

        if (extended_code) {
            extended_code = false; 
            return;
        }

        if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT) {
            shift_pressed = true;
            return;
        }
        if (scancode == SCANCODE_CAPS_LOCK) {
            caps_lock = !caps_lock;
            return;
        }

        char ascii = TranslateScancode(scancode);

        if (ascii != 0) {
            Input::add_kb(ascii);
        }
    }

    char Keyboard::TranslateScancode(uint8_t scancode) {
        if (scancode >= sizeof(scancode_set1_lowercase)) {
            return 0;
        }

        bool use_uppercase = shift_pressed ^ caps_lock;

        if (use_uppercase) {
            return scancode_set1_uppercase[scancode];
        } else {
            return scancode_set1_lowercase[scancode];
        }
    }

    void Keyboard::WaitWrite() {
        while (inb(STATUS_PORT) & STATUS_INPUT_BUFFER_FULL) {
            asm volatile("pause");
        }
    }

    void Keyboard::WaitRead() {
        while (!(inb(STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL)) {
            asm volatile("pause");
        }
    }

    void Keyboard::Initialize() {
        Debug::krnl_print("PS/2", Debug::LOG_INFO, "Initialize keyboard");
        
        WaitWrite();
        outb(COMMAND_PORT, CMD_DISABLE_FIRST_PORT);
        WaitWrite();
        outb(COMMAND_PORT, CMD_DISABLE_SECOND_PORT);

        while (inb(STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL) {
            inb(DATA_PORT);
        }

        WaitWrite();
        outb(COMMAND_PORT, CMD_READ_CONFIG_BYTE);
        WaitRead();
        uint8_t config = inb(DATA_PORT);

        config |= CONFIG_PORT1_INTERRUPT;
        config |= CONFIG_SYSTEM_POST_PASSED;
        config &= ~(1 << 4);
        config |= CONFIG_PORT1_TRANSLATION;

        WaitWrite();
        outb(COMMAND_PORT, CMD_WRITE_CONFIG_BYTE);
        WaitWrite();
        outb(DATA_PORT, config);

        WaitWrite();
        outb(COMMAND_PORT, CMD_ENABLE_FIRST_PORT);
        
        WaitWrite();
        outb(DATA_PORT, DEV_CMD_ENABLE_SCANNING);
        
        WaitRead();
        if (inb(DATA_PORT) == DEV_RESP_ACK) {
            Debug::krnl_print("PS/2", Debug::LOG_INFO, "Keyboard fully initialized");
        }
    }
}