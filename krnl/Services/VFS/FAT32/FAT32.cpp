#include <HAL/DISK/Disk.hpp>
#include <Library/debug.hpp>
#include <Services/VFS/FAT32/FAT32.hpp>
#include <stdint.h>
#include <stddef.h>
#include <Library/string.h>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>

using namespace HAL::MEM;

namespace VFS::FAT32 {
    bool FAT32FileSystem::initialize() {
        uint8_t sector_buffer[SCRATCH_BUF_SIZE];

        uint64_t part_lba = HAL::DISK::find_fat32_lba(m_disk_device->dev);
        
        Debug::krnl_print("FAT32", Debug::LOG_INFO, "Attempting sector read! [init]");
        m_disk_device->dev->read(part_lba, VOL_BOOT_REC_SIZE, sector_buffer);
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

        m_fat_start_sector += part_lba;
        m_data_start_sector += part_lba;

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
            Debug::krnl_print("FAT32", Debug::LOG_WARN, "FAT bad cluster!");
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

    uint32_t FAT32FileSystem::get_cluster_size() const {
        return m_bs.bytes_per_sector * m_bs.sectors_per_cluster;
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

    uint32_t FAT32FileSystem::get_bytes_per_sector() const {
        return m_bs.bytes_per_sector;
    }

    int FAT32VNode::write(uint64_t offset, const void *buffer, uint32_t size) {
        if (m_type != VFS::FileType::Regular) {
            return -1;
        }
        if (size == 0) {
            return 0;
        }

        uint32_t cluster_size = m_fs->get_cluster_size();
        uint32_t bytes_per_sector = m_fs->get_bytes_per_sector();
        const uint8_t* source_buffer = static_cast<const uint8_t*>(buffer);
        uint32_t total_bytes_written = 0;

        if (m_first_cluster == 0) {
            uint32_t free_cluster = 0;
            
            for (uint32_t c = 2; c < 0x0FFFFFFF; ++c) { 
                if (m_fs->read_FAT_entry(c) == 0) {
                    free_cluster = c;
                    break;
                }
            }
            if (free_cluster == 0) return -1;

            m_fs->write_FAT_entry(free_cluster, 0x0FFFFFFF); 
            m_first_cluster = free_cluster;
            m_is_dirty = true;
        }

        uint32_t current_cluster = m_first_cluster;
        uint32_t clusters_to_skip = offset / cluster_size;

        for (uint32_t i = 0; i < clusters_to_skip; ++i) {
            uint32_t next_cluster = m_fs->read_FAT_entry(current_cluster);
            
            if (next_cluster >= 0x0FFFFFF8) { 
                uint32_t free_cluster = 0;
                for (uint32_t c = 2; c < 0x0FFFFFFF; ++c) {
                    if (m_fs->read_FAT_entry(c) == 0) {
                        free_cluster = c;
                        break;
                    }
                }
                if (free_cluster == 0) return total_bytes_written;

                m_fs->write_FAT_entry(current_cluster, free_cluster);
                m_fs->write_FAT_entry(free_cluster, 0x0FFFFFFF);
                current_cluster = free_cluster;
            } else {
                current_cluster = next_cluster;
            }
        }

        uint8_t* sector_bounce_buffer = static_cast<uint8_t*>(
            HAL::MEM::PMEM::alloc_page(HAL::MEM::VMM::PTE_PRESENT | HAL::MEM::VMM::PTE_WRITABLE)
        );

        while (total_bytes_written < size) {
            uint32_t absolute_file_position = offset + total_bytes_written;
            uint32_t offset_within_cluster = absolute_file_position % cluster_size;

            uint32_t sector_offset_in_cluster = offset_within_cluster / bytes_per_sector;
            uint32_t byte_offset_in_sector = offset_within_cluster % bytes_per_sector;

            uint32_t physical_sector_lba = m_fs->cluster_to_sector(current_cluster) + sector_offset_in_cluster;

            uint32_t bytes_left_in_sector = bytes_per_sector - byte_offset_in_sector;
            uint32_t bytes_left_to_write = size - total_bytes_written;
            uint32_t chunk_size = (bytes_left_to_write < bytes_left_in_sector) ? bytes_left_to_write : bytes_left_in_sector;

            if (byte_offset_in_sector != 0 || chunk_size < bytes_per_sector) {
                m_fs->read_sectors(physical_sector_lba, sector_bounce_buffer, 1);
            }

            memcpy(sector_bounce_buffer + byte_offset_in_sector, source_buffer + total_bytes_written, chunk_size);

            if (m_fs->write_sectors(physical_sector_lba, sector_bounce_buffer, 1) < 1) {
                break;
            }

            total_bytes_written += chunk_size;

            if (total_bytes_written < size && (offset_within_cluster + chunk_size >= cluster_size)) {
                uint32_t next_cluster = m_fs->read_FAT_entry(current_cluster);
                if (next_cluster >= 0x0FFFFFF8) {
                    uint32_t free_cluster = 0;
                    for (uint32_t c = 2; c < 0x0FFFFFFF; ++c) {
                        if (m_fs->read_FAT_entry(c) == 0) {
                            free_cluster = c;
                            break;
                        }
                    }
                    if (free_cluster == 0) {
                        break;
                    }
                    m_fs->write_FAT_entry(current_cluster, free_cluster);
                    m_fs->write_FAT_entry(free_cluster, 0x0FFFFFFF);
                    current_cluster = free_cluster;
                } else {
                    current_cluster = next_cluster;
                }
            }
        }

        HAL::MEM::PMEM::free_page(sector_bounce_buffer);

        if (offset + total_bytes_written > m_size) {
            m_size = offset + total_bytes_written;
            m_is_dirty = true;
        }

        if (m_is_dirty && m_dir_cluster != 0) {
            uint8_t* dir_sector_buffer = static_cast<uint8_t*>(
                HAL::MEM::PMEM::alloc_page(HAL::MEM::VMM::PTE_PRESENT | HAL::MEM::VMM::PTE_WRITABLE)
            );

            uint32_t dir_sector_lba = m_fs->cluster_to_sector(m_dir_cluster) + (m_dir_offset / bytes_per_sector);
            uint32_t entry_offset_in_sector = m_dir_offset % bytes_per_sector;

            if (m_fs->read_sectors(dir_sector_lba, dir_sector_buffer, 1) >= 1) {
                DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(&dir_sector_buffer[entry_offset_in_sector]);
                
                entry->file_size = static_cast<uint32_t>(m_size);
                
                entry->cluster_high = static_cast<uint16_t>((m_first_cluster >> 16) & 0xFFFF); 
                entry->cluster_low = static_cast<uint16_t>(m_first_cluster & 0xFFFF);        

                m_fs->write_sectors(dir_sector_lba, dir_sector_buffer, 1);
                m_is_dirty = false;
            }

            HAL::MEM::PMEM::free_page(dir_sector_buffer);
        }

        return total_bytes_written;
    }

    VFS::VNode *FAT32VNode::create(const char *name, VFS::FileType type) {
        if (m_type != VFS::FileType::Directory) {
            return nullptr;
        }

        VFS::VNode *check_exists = lookup(name);
        if (check_exists != nullptr) {
            return nullptr;
        }

        uint8_t target_83_name[FAT_83_NAME_SIZE];
        for (int i = START_INDEX; i < (int)FAT_83_NAME_SIZE; ++i) {
            target_83_name[i] = ' ';
        }

        int source_index = START_INDEX;
        int destination_index = START_INDEX;
        while (name[source_index] != '\0' && name[source_index] != '.' && destination_index < MAX_SHORT_NAME) {
            char character = name[source_index];
            if (character >= 'a' && character <= 'z') {
                character -= CASE_CONVERSION_OFF;
            }
            target_83_name[destination_index++] = character;
            source_index++;
        }

        if (name[source_index] == '.') {
            source_index++;
            destination_index = EXTENSION_START_IDX;
            while (name[source_index] != '\0' && destination_index < (int)FAT_83_NAME_SIZE) {
                char character = name[source_index];
                if (character >= 'a' && character <= 'z') {
                    character -= CASE_CONVERSION_OFF;
                }
                target_83_name[destination_index++] = character;
                source_index++;
            }
        }

        uint32_t bytes_per_sector = m_fs->get_bytes_per_sector();
        uint32_t sectors_per_cluster = m_fs->get_cluster_size() / bytes_per_sector;
        uint32_t current_cluster = m_first_cluster;

        uint8_t *sector_buffer = static_cast<uint8_t *>(
            HAL::MEM::PMEM::alloc_page(HAL::MEM::VMM::PTE_PRESENT | HAL::MEM::VMM::PTE_WRITABLE)
        );

        while (true) {
            uint32_t cluster_base_sector = m_fs->cluster_to_sector(current_cluster);

            for (uint32_t sector_offset = START_INDEX; sector_offset < sectors_per_cluster; ++sector_offset) {
                uint32_t physical_sector_lba = cluster_base_sector + sector_offset;
                m_fs->read_sectors(physical_sector_lba, sector_buffer, SECTOR_COUNT_ONE);

                uint32_t directory_entries_per_sector = bytes_per_sector / sizeof(DirectoryEntry);
                DirectoryEntry *entry_array = reinterpret_cast<DirectoryEntry *>(sector_buffer);

                for (uint32_t entry_index = START_INDEX; entry_index < directory_entries_per_sector; ++entry_index) {
                    DirectoryEntry &entry = entry_array[entry_index];
                    uint8_t initial_byte = static_cast<uint8_t>(entry.name[FIRST_BYTE_INDEX]);

                    if (initial_byte == DIR_ENTRY_FREE|| initial_byte == DIR_ENTRY_FREE_ONWARD) {
                        memcpy(entry.name, target_83_name, FAT_83_NAME_SIZE);
                        entry.attr = (type == VFS::FileType::Directory) ? ATTR_DIRECTORY : ATTR_ARCHIVE;
                        entry.file_size = INITIAL_SIZE;
                        entry.cluster_high = INITIAL_CLUSTER_HI;
                        entry.cluster_low = INITIAL_CLUSTER_LO;

                        m_fs->write_sectors(physical_sector_lba, sector_buffer, SECTOR_COUNT_ONE);
                        HAL::MEM::PMEM::free_page(sector_buffer);

                        uint32_t intra_sector_byte_offset = entry_index * sizeof(DirectoryEntry);
                        uint32_t total_dir_offset = (sector_offset * bytes_per_sector) + intra_sector_byte_offset;

                        return new FAT32VNode(
                            m_fs,
                            type,
                            INITIAL_SIZE,
                            INITIAL_CLUSTER,
                            current_cluster,
                            total_dir_offset
                        );
                    }
                }
            }

            uint32_t next_cluster = m_fs->read_FAT_entry(current_cluster);
            if (next_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) {
                uint32_t free_cluster = INITIAL_FREE_CLUSTER;
                for (uint32_t c = FIRST_DATA_CLUSTER; c < CLUSTER_MAX; ++c) {
                    if (m_fs->read_FAT_entry(c) == CLUSTER_FREE) {
                        free_cluster = c;
                        break;
                    }
                }

                if (free_cluster == ALLOC_FAILED) {
                    break;
                }

                m_fs->write_FAT_entry(current_cluster, free_cluster);
                m_fs->write_FAT_entry(free_cluster, CLUSTER_EOF_MAX);

                uint8_t *zero_buffer = static_cast<uint8_t *>(
                    HAL::MEM::PMEM::alloc_page(HAL::MEM::VMM::PTE_PRESENT | HAL::MEM::VMM::PTE_WRITABLE)
                );
                memset(zero_buffer, 0, bytes_per_sector);

                uint32_t new_cluster_base_sector = m_fs->cluster_to_sector(free_cluster);
                for (uint32_t sector_offset = START_INDEX; sector_offset < sectors_per_cluster; ++sector_offset) {
                    m_fs->write_sectors(new_cluster_base_sector + sector_offset, zero_buffer, SECTOR_COUNT_ONE);
                }
                HAL::MEM::PMEM::free_page(zero_buffer);

                current_cluster = free_cluster;
            } else {
                current_cluster = next_cluster;
            }
        }

        HAL::MEM::PMEM::free_page(sector_buffer);
        return nullptr;
    }

    VFS::VNode* FAT32FileSystem::get_root_node() {
        uint64_t directory_size_placeholder = 0;
        uint32_t root_directory_parent_cluster = 0;
        uint32_t root_directory_parent_offset = 0;

        return new FAT32VNode(
            this,
            VFS::FileType::Directory,
            directory_size_placeholder,
            m_bs.root_cluster,
            root_directory_parent_cluster,
            root_directory_parent_offset
        );
    }
    
    FAT32FileSystem::FAT32FileSystem(HAL::DISK::Disk *disk_device) : m_disk_device(disk_device) {}

    int FAT32VNode::read(uint64_t offset, void *buffer, uint32_t size) {
        uint32_t safe_read_size = size;
        if (m_type == VFS::FileType::Regular) {
            if (offset >= m_size) {
                Debug::krnl_print("FAT32", Debug::LOG_WARN, "Offset greater than size (%i)!", m_size);
                return 0;
            }
            if (offset + size > m_size) {
                safe_read_size = m_size - offset;
            }
        }

        uint32_t cluster_size = m_fs->get_cluster_size(); 
        uint32_t current_cluster = m_first_cluster;
        uint32_t clusters_to_skip = offset / cluster_size;

        for (uint32_t i = 0; i < clusters_to_skip; ++i) {
            current_cluster = m_fs->read_FAT_entry(current_cluster);
            if (current_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) {
                return 0; 
            }
        }

        uint8_t* destination_buffer = static_cast<uint8_t*>(buffer);
        uint32_t total_bytes_read = 0;

        uint32_t bytes_per_sector = m_fs->get_bytes_per_sector(); 
        uint8_t* sector_bounce_buffer = static_cast<uint8_t*>(
            HAL::MEM::PMEM::alloc_page(HAL::MEM::VMM::PTE_PRESENT | HAL::MEM::VMM::PTE_WRITABLE)
        );

        while (total_bytes_read < safe_read_size && current_cluster < VFS::FAT32::CLUSTER_EOF_MIN) {
            uint32_t absolute_file_position = offset + total_bytes_read;
            uint32_t offset_within_cluster = absolute_file_position % cluster_size;

            uint32_t sector_offset_in_cluster = offset_within_cluster / bytes_per_sector;
            uint32_t byte_offset_in_sector = offset_within_cluster % bytes_per_sector;

            uint32_t physical_sector_lba = m_fs->cluster_to_sector(current_cluster) + sector_offset_in_cluster;

            uint32_t bytes_left_in_sector = bytes_per_sector - byte_offset_in_sector;
            uint32_t bytes_left_to_read = safe_read_size - total_bytes_read;
            uint32_t chunk_size = (bytes_left_to_read < bytes_left_in_sector) ? bytes_left_to_read : bytes_left_in_sector;

            if (m_fs->read_sectors(physical_sector_lba, sector_bounce_buffer, 1) < 1) {
                break;
            }

            memcpy(destination_buffer + total_bytes_read, sector_bounce_buffer + byte_offset_in_sector, chunk_size);

            total_bytes_read += chunk_size;

            if (offset_within_cluster + chunk_size >= cluster_size) {
                current_cluster = m_fs->read_FAT_entry(current_cluster);
            }
        }

        HAL::MEM::PMEM::free_page(sector_bounce_buffer);
        return total_bytes_read;
    }

    HAL::DISK::Disk *FAT32FileSystem::get_disk_device() const {
        return m_disk_device;
    }

    VFS::VNode* FAT32VNode::lookup(const char* name) {
        if (m_type != VFS::FileType::Directory) {
            return nullptr;
        }

        uint8_t target_83_name[11];
        for (int i = 0; i < 11; ++i) {
            target_83_name[i] = ' ';
        }

        int source_index = 0;
        int destination_index = 0;
        while (name[source_index] != '\0' && name[source_index] != '.' && destination_index < 8) {
            char character = name[source_index];
            if (character >= 'a' && character <= 'z') {
                character -= 32;
            }
            target_83_name[destination_index++] = character;
            source_index++;
        }

        if (name[source_index] == '.') {
            source_index++;
            destination_index = 8;
            while (name[source_index] != '\0' && destination_index < 11) {
                char character = name[source_index];
                if (character >= 'a' && character <= 'z') {
                    character -= 32;
                }
                target_83_name[destination_index++] = character;
                source_index++;
            }
        }

        uint32_t bytes_per_sector = m_fs->get_bytes_per_sector();
        uint32_t sectors_per_cluster = m_fs->get_cluster_size() / bytes_per_sector;
        uint32_t current_cluster = m_first_cluster;

        uint8_t* sector_buffer = static_cast<uint8_t*>(
            HAL::MEM::PMEM::alloc_page(HAL::MEM::VMM::PTE_PRESENT | HAL::MEM::VMM::PTE_WRITABLE)
        );

        while (current_cluster < VFS::FAT32::CLUSTER_EOF_MIN) {
            uint32_t cluster_base_sector = m_fs->cluster_to_sector(current_cluster);

            for (uint32_t sector_offset = 0; sector_offset < sectors_per_cluster; ++sector_offset) {
                uint32_t physical_sector_lba = cluster_base_sector + sector_offset;

                m_fs->read_sectors(physical_sector_lba, sector_buffer, 1);

                uint32_t directory_entries_per_sector = bytes_per_sector / sizeof(DirectoryEntry);
                DirectoryEntry* entry_array = reinterpret_cast<DirectoryEntry*>(sector_buffer);

                for (uint32_t entry_index = 0; entry_index < directory_entries_per_sector; ++entry_index) {
                    DirectoryEntry& entry = entry_array[entry_index];
                    uint8_t initial_name_byte = static_cast<uint8_t>(entry.name[0]);

                    if (initial_name_byte == VFS::FAT32::DIR_ENTRY_FREE_ONWARD) { 
                        HAL::MEM::PMEM::free_page(sector_buffer);
                        return nullptr;
                    }
                    if (initial_name_byte == VFS::FAT32::DIR_ENTRY_FREE) { 
                        continue;
                    }
                    if (entry.attr == VFS::FAT32::ATTR_LONG_NAME) { 
                        continue;
                    }

                    bool identity_matches = true;
                    for (int byte_index = 0; byte_index < 11; ++byte_index) {
                        if (static_cast<uint8_t>(entry.name[byte_index]) != target_83_name[byte_index]) {
                            identity_matches = false;
                            break;
                        }
                    }

                    if (identity_matches) {
                        uint32_t entry_target_cluster = entry.GetFirstCluster();

                        VFS::FileType calculated_type = VFS::FileType::Regular;
                        if (entry.attr & VFS::FAT32::ATTR_DIRECTORY) { 
                            calculated_type = VFS::FileType::Directory;
                        }

                        uint32_t intra_sector_byte_offset = entry_index * sizeof(DirectoryEntry);

                        VFS::VNode* discovered_node = new FAT32VNode(
                            m_fs,
                            calculated_type,
                            entry.file_size,
                            entry_target_cluster,
                            current_cluster,
                            intra_sector_byte_offset
                        );

                        HAL::MEM::PMEM::free_page(sector_buffer);
                        return discovered_node;
                    }
                }
            }

            current_cluster = m_fs->read_FAT_entry(current_cluster);
        }

        HAL::MEM::PMEM::free_page(sector_buffer);
        return nullptr;
    }

    FAT32VNode::FAT32VNode(
        FAT32FileSystem* fs, 
        VFS::FileType type, 
        uint64_t size, 
        uint32_t first_cluster, 
        uint32_t dir_cluster, 
        uint32_t dir_offset
    ) : VNode(type, size),
        m_fs(fs),
        m_first_cluster(first_cluster),
        m_dir_cluster(dir_cluster),
        m_dir_offset(dir_offset),
        m_is_dirty(false) {}

    int FAT32FileSystem::read_sectors(uint64_t sector, void* buffer, uint32_t count) {
        return m_disk_device->dev->read(sector, count, buffer);
    }

    int FAT32FileSystem::write_sectors(uint64_t sector, const void* buffer, uint32_t count) {
        return m_disk_device->dev->write(sector, count, const_cast<void *>(buffer));
    }
}