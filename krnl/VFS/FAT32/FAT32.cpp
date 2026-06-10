#include "HAL/DISK/Disk.hpp"
#include "Library/debug.hpp"
#include <VFS/FAT32/FAT32.hpp>
#include <stdint.h>
#include <stddef.h>
#include <Library/string.h>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>

using namespace HAL::MEM;

namespace VFS::FAT32 {
    bool FAT32FileSystem::initialize() {
        uint8_t sector_buffer[SCRATCH_BUF_SIZE];
        
        Debug::krnl_print("FAT32", Debug::LOG_INFO, "Attempting sector read! [init]");
        m_disk_device->dev->read(HAL::DISK::find_fat32_lba(m_disk_device->dev), VOL_BOOT_REC_SIZE, sector_buffer);
        Debug::krnl_print("FAT32", Debug::LOG_INFO, "Passed sector read!");
        memcpy(&m_bs, sector_buffer, sizeof(BootSector));

        if (m_bs.boot_sector_signature != BS_SIGNATURE) {
            return false;
        }

        if (m_bs.root_entry_count != 0 || m_bs.fat_size_16 != 0) {
            return false;
        }

        m_fat_start_sector = m_bs.reserved_sector_count;

        uint32_t total_fat_size = m_bs.num_fats * m_bs.fat_size_32;
        m_data_start_sector = m_fat_start_sector + total_fat_size;

        uint32_t total_sectors = (m_bs.total_sectors_16 != 0) ? m_bs.total_sectors_16 : m_bs.total_sectors_32;
        uint32_t data_sectors = total_sectors - m_data_start_sector;

        m_total_clusters = data_sectors / m_bs.sectors_per_cluster;

        Debug::krnl_print("FAT32", Debug::LOG_INFO, "Verifying additional clusters");
        if (m_bs.fs_info_sector != 0) {
            m_disk_device->dev->read(m_bs.fs_info_sector, 1, sector_buffer);
            memcpy(&m_fsi, sector_buffer, sizeof(FSInfo));
            
            if (m_fsi.lead_signature != FSINFO_LEAD_SIG || m_fsi.struct_signature != FSINFO_STRU_SIG) {
                m_bs.fs_info_sector = 0; 
            }
        }

        return true;
    }

    uint32_t FAT32FileSystem::cluster_to_sector(uint32_t cluster) const {
        if (cluster < 2) return 0;
        return m_data_start_sector + ((cluster - 2) * m_bs.sectors_per_cluster);
    }

    uint32_t FAT32FileSystem::read_FAT_entry(uint32_t cluster) {
        if (cluster >= m_total_clusters + 2) {
            return CLUSTER_BAD;
        }

        uint32_t fat_offset = cluster * 4;
        uint32_t sector = m_fat_start_sector + (fat_offset / m_bs.bytes_per_sector);
        uint32_t entry_offset = fat_offset % m_bs.bytes_per_sector;

        uint8_t *sector_buffer = static_cast<uint8_t *>(PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE));

        m_disk_device->dev->read(sector, 1, sector_buffer);

        uint32_t raw_entry = *reinterpret_cast<uint32_t *>(&sector_buffer[entry_offset]);
        PMEM::free_page(sector_buffer);
        return raw_entry & CLUSTER_MASK;
    }

    bool FAT32FileSystem::write_FAT_entry(uint32_t cluster, uint32_t val) {
        if (cluster >= m_total_clusters + 2) {
            return false;
        }

        uint32_t fat_offset = cluster * 4;
        uint32_t sector_offset_from_fat_start = fat_offset / m_bs.bytes_per_sector;
        uint32_t entry_offset = fat_offset % m_bs.bytes_per_sector;

        uint8_t *sector_buffer = static_cast<uint8_t *>(PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE));

        for (uint32_t i = 0; i < m_bs.num_fats; i++) {
            uint32_t active_fat_start = m_fat_start_sector + (i * m_bs.fat_size_32);
            uint32_t actual_sector = active_fat_start + sector_offset_from_fat_start;

            m_disk_device->dev->read(actual_sector, 1, sector_buffer);

            uint32_t* entry_ptr = reinterpret_cast<uint32_t*>(&sector_buffer[entry_offset]);
            *entry_ptr = (*entry_ptr & 0xF0000000) | (val & CLUSTER_MASK);

            m_disk_device->dev->write(actual_sector, 1, sector_buffer);
        }

        PMEM::free_page(sector_buffer);

        return true;
    }

    VFS::VNode* FAT32FileSystem::get_root_node() {
        return nullptr;
    }
    
    FAT32FileSystem::FAT32FileSystem(HAL::DISK::Disk *disk_device) : m_disk_device(disk_device) {

    }
}