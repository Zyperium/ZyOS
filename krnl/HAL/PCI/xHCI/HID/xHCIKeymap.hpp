#pragma once
#include <stdint.h>

namespace HAL::PCI::HID {
    struct KeyPair { 
        char normal; 
        char shifted; 
    };
    class USBKeymap {
    private:
        static KeyPair layout[256];
        static bool is_initialized;

    public:
        static void Initialize();
        static KeyPair Get(uint8_t scancode);
    };
}