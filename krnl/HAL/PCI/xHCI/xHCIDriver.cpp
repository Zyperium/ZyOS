#include <HAL/PCI/xHCI/xHCIDriver.hpp>
#include <HAL/DISK/USB/USBStorage.hpp>

namespace HAL::PCI {
    xHCIDriver *LoadNewDriver(uint8_t classCode, uint8_t subClass, uint8_t protocol) {
        if (classCode == CLASS_USB_HUB) {
            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Discovered USB Hub");
            return nullptr;
        }

        if (classCode == CLASS_HID) {
            if (subClass == BOOT_IFACE_SUBCLASS) {
                if (protocol == PROT_HID_KEYBOARD) {
                    Debug::krnl_print("xHCI", Debug::LOG_INFO, "Discovered USB Keyboard");
                    return nullptr;
                }
                else if (protocol == PROT_HID_MOUSE) {
                    Debug::krnl_print("xHCI", Debug::LOG_INFO, "Discovered USB Mouse");
                    return nullptr;
                }
            }
        }

        if (classCode == CLASS_MASS_STORAGE) {
            Debug::krnl_print("xHCI", Debug::LOG_INFO, "Discovered USB Mass Storage");
            return new DISK::USB::USBStorage;
        }

        return nullptr;
    }
}