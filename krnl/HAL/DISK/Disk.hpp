#pragma once
#include <stdint.h>

namespace VFS {
    class VNode;
}

namespace HAL::DISK {
    

    class DiskDevice {
    public:
        virtual void read(uint64_t sector, uint32_t count, void *buffer) = 0;
        virtual void write(uint64_t sector, uint32_t count, void* buffer) = 0;
    };

    class Disk {
    public:
        char drv_ltr{0};
        VFS::VNode *rootnode;
        DiskDevice *dev;

        bool initializefs();
        bool is_initialized();
        static Disk *CreateDisk(DiskDevice *device, char letter = 0);
    private:
        static bool found_bootdisk;
        bool initialized{};
        Disk(char letter, DiskDevice *dev);
    };

    bool RegisterDisk(Disk disk);
    VFS::VNode *GetRootOfDrive(char drive);
    bool IsCapital(char ch);
    char GetValidDriveLabel();
    bool IsValidDisk(char ch);
    void InitDiskData();

    constexpr uint32_t SIZE_OF_UUID = 16;

    struct UUID {
        uint8_t bytes[SIZE_OF_UUID];
    };

    constexpr uint16_t MAX_DISKS = 26;

    struct MBRPartitionEntry {
        uint8_t  status;
        uint8_t  chs_first[3];
        uint8_t  type;
        uint8_t  chs_last[3];
        uint32_t lba_start;
        uint32_t sector_count;
    } __attribute__((packed));

    struct GPTHeader {
        uint64_t signature;
        uint32_t revision;
        uint32_t header_size;
        uint32_t header_crc32;
        uint32_t reserved;
        uint64_t my_lba;
        uint64_t alternate_lba;
        uint64_t first_usable_lba;
        uint64_t last_usable_lba;
        uint8_t  disk_guid[16];
        uint64_t partition_entry_lba;
        uint32_t num_partition_entries;
        uint32_t sizeof_partition_entry;
        uint32_t partition_entry_array_crc32;
    } __attribute__((packed));

    struct GPTPartitionEntry {
        uint8_t  partition_type_guid[16];
        uint8_t  unique_partition_guid[16];
        uint64_t starting_lba;
        uint64_t ending_lba;
        uint64_t attributes;
        uint16_t partition_name[36];
    } __attribute__((packed));

    uint64_t find_fat32_lba(DiskDevice* dev);
}