#pragma once
#include <stdint.h>
#include <stddef.h>
#include <VFS/VFS.hpp>
#include <HAL/DISK/Disk.hpp>

namespace VFS::FAT32 {
    constexpr uint16_t BS_SIGNATURE         = 0xAA55;
    constexpr uint32_t FSINFO_LEAD_SIG      = 0x41564244;
    constexpr uint32_t FSINFO_STRU_SIG      = 0x61417272;

    constexpr uint32_t CLUSTER_FREE         = 0x00000000;
    constexpr uint32_t CLUSTER_RESERVED     = 0x0FFFFFF0;
    constexpr uint32_t CLUSTER_BAD          = 0x0FFFFFF7;
    constexpr uint32_t CLUSTER_EOF_MIN      = 0x0FFFFFF8;
    constexpr uint32_t CLUSTER_EOF_MAX      = 0x0FFFFFFF;
    constexpr uint32_t CLUSTER_MASK         = 0x0FFFFFFF;

    constexpr uint8_t ATTR_READ_ONLY        = 0x01;
    constexpr uint8_t ATTR_HIDDEN           = 0x02;
    constexpr uint8_t ATTR_SYSTEM           = 0x04;
    constexpr uint8_t ATTR_VOLUME_ID        = 0x08;
    constexpr uint8_t ATTR_DIRECTORY        = 0x10;
    constexpr uint8_t ATTR_ARCHIVE          = 0x20;
    constexpr uint8_t ATTR_LONG_NAME        = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;

    constexpr size_t DIR_ENTRY_SIZE         = 32;
    constexpr uint8_t DIR_ENTRY_FREE_ONWARD = 0x00;
    constexpr uint8_t DIR_ENTRY_FREE        = 0xE5;
    
    constexpr uint32_t SCRATCH_BUF_SIZE     = 512;
    constexpr uint32_t VOL_BOOT_REC_SECT    = 0;
    constexpr uint32_t VOL_BOOT_REC_SIZE    = 1;

    #pragma pack(push, 1)

    struct BootSector {
        uint8_t  jmp_boot[3];
        char     oem_name[8];
        uint16_t bytes_per_sector;
        uint8_t  sectors_per_cluster;
        uint16_t reserved_sector_count;
        uint8_t  num_fats;
        uint16_t root_entry_count;
        uint16_t total_sectors_16;
        uint8_t  media_type;
        uint16_t fat_size_16;
        uint16_t sectors_per_track;
        uint16_t num_heads;
        uint32_t hidden_sectors;
        uint32_t total_sectors_32;
        uint32_t fat_size_32;
        uint16_t ext_flags;
        uint16_t fs_version;
        uint32_t root_cluster;
        uint16_t fs_info_sector;
        uint16_t backup_boot_sector;
        uint8_t  reserved[12];
        uint8_t  drive_number;
        uint8_t  reserved1;
        uint8_t  boot_signature;
        uint32_t volume_id;
        char     volume_label[11];
        char     file_system_type[8];
        uint8_t  boot_code[420];
        uint16_t boot_sector_signature;
    };

    struct FSInfo {
        uint32_t lead_signature;
        uint8_t  reserved1[480];
        uint32_t struct_signature;
        uint32_t free_count;
        uint32_t next_free;
        uint8_t  reserved2[12];
        uint32_t trail_signature;
    };

    struct DirectoryEntry {
        char     name[11];
        uint8_t  attr;
        uint8_t  nt_res;
        uint8_t  creation_time_tenth;
        uint16_t creation_time;
        uint16_t creation_date;
        uint16_t last_access_date;
        uint16_t cluster_high;
        uint16_t write_time;
        uint16_t write_date;
        uint16_t cluster_low;
        uint32_t file_size;
        uint32_t GetFirstCluster() const {
            return (static_cast<uint32_t>(cluster_high) << 16) | cluster_low;
        }
    };

    struct LongNameEntry {
        uint8_t order;
        char16_t name1[5];
        uint8_t attr;
        uint8_t type;
        uint8_t checksum;
        char16_t name2[6];
        uint16_t first_cluster;
        char16_t name3[2];
    };

    #pragma pack(pop)

    class FAT32FileSystem;
    
    class FAT32VNode : public VNode {
    private:
        FAT32FileSystem *m_fs;
        uint32_t m_first_cluster;
        uint32_t m_dir_cluster;
        uint32_t m_dir_offset;
        bool m_is_dirty;

    public:
        FAT32VNode(FAT32FileSystem* fs, VFS::FileType type, uint64_t size, uint32_t first_cluster, uint32_t dir_cluster, uint32_t dir_offset);
        virtual ~FAT32VNode() override = default;

        virtual int read(uint64_t offset, void *buffer, uint32_t size) override;
        virtual int write(uint64_t offset, const void *buffer, uint32_t size) override;
        virtual VFS::VNode* lookup(const char *name) override;

        uint32_t GetFirstCluster() const { return m_first_cluster; }
    };

    class FAT32FileSystem {
    private:
        HAL::DISK::Disk *m_disk_device;
        BootSector m_bs;
        FSInfo m_fsi;

        uint32_t m_fat_start_sector;
        uint32_t m_data_start_sector;
        uint32_t m_total_clusters;
    public:
        FAT32FileSystem(HAL::DISK::Disk *disk_dev);
        ~FAT32FileSystem() = default;
        bool initialize();
        int read_sectors(uint64_t sector, void* buffer, uint32_t count);
        int write_sectors(uint64_t sector, const void* buffer, uint32_t count);

        uint32_t cluster_to_sector(uint32_t cluster) const;
        uint32_t read_FAT_entry(uint32_t cluster);
        uint32_t get_cluster_size() const;
        uint32_t get_bytes_per_sector() const;
        bool write_FAT_entry(uint32_t cluster, uint32_t value);
        VFS::VNode* get_root_node();
        HAL::DISK::Disk *get_disk_device() const;
    };
}