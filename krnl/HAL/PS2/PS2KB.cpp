#include <HAL/PS2/PS2KB.hpp>
#include <Library/io.hpp>
#include <Library/debug.hpp>
#include <Services/TTY/TTY.hpp>
/**
This is a really fast PS2 driver to test the xHCI driver in more depth.
Please, please ignore the magic numbers. I'll either remove this driver later,
or fix the magic. Realistically, who is using PS2 in 2026? (Laptop keyboards, funnily enough)
*/

extern "C" void KBHI_Wrapper() {
    PS2::Keyboard::HandleInterrupt();
    return;
}

namespace PS2 {
    bool Keyboard::shift_pressed{};
    bool Keyboard::caps_lock{};
    bool Keyboard::extended_code{};

    void Keyboard::HandleInterrupt() {
        if (!(inb(STATUS_PORT) & 1)) {
            return; 
        }

        uint8_t scancode = inb(DATA_PORT);

        if (scancode == 0xE0) {
            extended_code = true;
            return;
        }

        if (scancode & 0x80) {
            uint8_t make_code = scancode & 0x7F;
            
            if (make_code == 0x2A || make_code == 0x36) {
                shift_pressed = false;
            }

            extended_code = false;
            return;
        }

        if (extended_code) {
            extended_code = false; 
            return;
        }

        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
            return;
        }
        if (scancode == 0x3A) {
            caps_lock = !caps_lock;
            return;
        }

        char ascii = TranslateScancode(scancode);

        if (ascii != 0) {
            TTY::conhosts[TTY::active_host]->send_input(ascii);
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
        while (inb(STATUS_PORT) & 2) {
            asm volatile("pause");
        }
    }

    void Keyboard::WaitRead() {
        while (!(inb(STATUS_PORT) & 1)) {
            asm volatile("pause");
        }
    }

    void Keyboard::Initialize() {
        Debug::krnl_print("PS/2", Debug::LOG_INFO, "Initialize keyboard");
        WaitWrite();
        outb(STATUS_PORT, 0xAD);
        WaitWrite();
        outb(STATUS_PORT, 0xA7);

        while (inb(STATUS_PORT) & 1) {
            inb(DATA_PORT);
        }

        WaitWrite();
        outb(STATUS_PORT, 0x20);
        WaitRead();
        uint8_t config = inb(DATA_PORT);

        config |= (1 << 0);
        config |= (1 << 2);
        config &= ~(1 << 4);
        config |= (1 << 6);

        WaitWrite();
        outb(STATUS_PORT, 0x60);
        WaitWrite();
        outb(DATA_PORT, config);
        
        WaitWrite();
        outb(STATUS_PORT, 0xAE);
        
        WaitWrite();
        outb(DATA_PORT, 0xF4);
        
        WaitRead();
        if (inb(DATA_PORT) == 0xFA) {
            Debug::krnl_print("PS/2", Debug::LOG_INFO, "Keyboard fully initialized");
        }
    }
}