#pragma once
#include <stdint.h>

namespace HAL::DISK {
    class VNode;

    class DiskDevice {
    public:
        virtual void read(uint64_t sector, uint32_t count, void *buffer) = 0;
        virtual void write(uint64_t sector, uint32_t count, void* buffer) = 0;
    };

    class Disk {
    public:
        char drv_ltr{0};
        VNode *rootnode;
        DiskDevice *dev;

        bool initializefs();
        bool is_initialized();
        static Disk *CreateDisk(DiskDevice *device, char letter = 0);
    private:
        bool initialized;
        Disk(char letter, DiskDevice *dev);
    };

    bool RegisterDisk(Disk disk);
    VNode *GetRootOfDrive(char drive);
    bool IsCapital(char ch);
    char GetValidDriveLabel();

    constexpr uint16_t MAX_DISKS = 26;
}