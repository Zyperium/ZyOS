#include <HAL/DISK/Disk.hpp>
#include <Library/debug.hpp>

namespace HAL::DISK {
    Disk *disks[MAX_DISKS]{nullptr};

    Disk::Disk(char letter, DiskDevice *dev) : drv_ltr(letter), dev(dev) {}

    bool Disk::initializefs() {
        // this needs to let the VFS' test the drive for a valid Filesystem.
        if (initialized) {
            Debug::krnl_print("DISK", Debug::LOG_WARN, "Reinitializing disk!");
        }

        initialized = true;
        return false;
    }

    bool Disk::is_initialized() {
        return initialized;
    }

    Disk *Disk::CreateDisk(DiskDevice *dev, char letter) {
        if (!IsCapital(letter)) {
            letter = GetValidDriveLabel();
            if (!letter) return nullptr;
        }

        if (disks[letter - 'A']) {
            letter = GetValidDriveLabel();
            if (!letter) return nullptr;
        }

        Disk *n_disk = new Disk(letter, dev);

        Debug::krnl_print("DISK", Debug::LOG_INFO, "New disk successfully initialized. Remember to initialize the FS!");
        return n_disk;
    }

    bool RegisterDisk() {
        return false;
    }

    bool IsCapital(char ch) {
        return (ch >= 'A' && ch <= 'Z');
    }

    char GetValidDriveLabel() {
        for (auto i{0uz}; i < MAX_DISKS; i++) {
            if (!disks[i]) return 'A' + i;
        }

        return 0;
    }
}