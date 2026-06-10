#include <HAL/PCI/xHCI/HID/xHCIKeymap.hpp>

namespace HAL::PCI::HID {
    KeyPair USBKeymap::layout[256] = {};
    bool USBKeymap::is_initialized = false;

    void USBKeymap::Initialize() {
        if (is_initialized) return;

        layout[0x04] = {'a', 'A'}; layout[0x05] = {'b', 'B'}; layout[0x06] = {'c', 'C'};
        layout[0x07] = {'d', 'D'}; layout[0x08] = {'e', 'E'}; layout[0x09] = {'f', 'F'};
        layout[0x0A] = {'g', 'G'}; layout[0x0B] = {'h', 'H'}; layout[0x0C] = {'i', 'I'};
        layout[0x0D] = {'j', 'J'}; layout[0x0E] = {'k', 'K'}; layout[0x0F] = {'l', 'L'};
        layout[0x10] = {'m', 'M'}; layout[0x11] = {'n', 'N'}; layout[0x12] = {'o', 'O'};
        layout[0x13] = {'p', 'P'}; layout[0x14] = {'q', 'Q'}; layout[0x15] = {'r', 'R'};
        layout[0x16] = {'s', 'S'}; layout[0x17] = {'t', 'T'}; layout[0x18] = {'u', 'U'};
        layout[0x19] = {'v', 'V'}; layout[0x1A] = {'w', 'W'}; layout[0x1B] = {'x', 'X'};
        layout[0x1C] = {'y', 'Y'}; layout[0x1D] = {'z', 'Z'};

        layout[0x1E] = {'1', '!'}; layout[0x1F] = {'2', '@'}; layout[0x20] = {'3', '#'};
        layout[0x21] = {'4', '$'}; layout[0x22] = {'5', '%'}; layout[0x23] = {'6', '^'};
        layout[0x24] = {'7', '&'}; layout[0x25] = {'8', '*'}; layout[0x26] = {'9', '('};
        layout[0x27] = {'0', ')'};

        layout[0x28] = {'\n','\n'};
        layout[0x29] = {0x1B, 0x1B};
        layout[0x2A] = {'\b','\b'};
        layout[0x2B] = {'\t','\t'};
        layout[0x2C] = {' ', ' '};
        layout[0x2D] = {'-', '_'};
        layout[0x2E] = {'=', '+'};
        layout[0x2F] = {'[', '{'};
        layout[0x30] = {']', '}'};
        layout[0x31] = {'\\', '|'};
        layout[0x33] = {';', ':'};
        layout[0x34] = {'\'', '"'};
        layout[0x35] = {'`', '~'};
        layout[0x36] = {',', '<'};
        layout[0x37] = {'.', '>'};
        layout[0x38] = {'/', '?'};

        layout[0x54] = {'/', '/'};  
        layout[0x55] = {'*', '*'};  
        layout[0x56] = {'-', '-'};
        layout[0x57] = {'+', '+'};  
        layout[0x58] = {'\n','\n'}; 
        layout[0x59] = {'1', '1'};
        layout[0x5A] = {'2', '2'};  
        layout[0x5B] = {'3', '3'};  
        layout[0x5C] = {'4', '4'};
        layout[0x5D] = {'5', '5'};  
        layout[0x5E] = {'6', '6'};  
        layout[0x5F] = {'7', '7'};
        layout[0x60] = {'8', '8'};  
        layout[0x61] = {'9', '9'};  
        layout[0x62] = {'0', '0'};
        layout[0x63] = {'.', '.'};
        is_initialized = true;
    }

    KeyPair USBKeymap::Get(uint8_t scancode) {
        if (!is_initialized) {
            Initialize();
        }
        return layout[scancode];
    }
}