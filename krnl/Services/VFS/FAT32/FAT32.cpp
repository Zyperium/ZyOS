#include <HAL/DISK/Disk.hpp>
#include <Library/debug.hpp>
#include <Library/io.hpp>
#include <Services/VFS/FAT32/FAT32.hpp>
#include <stdint.h>
#include <stddef.h>
#include <Library/string.h>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>

using namespace HAL::MEM;

namespace VFS::FAT32 {

    constexpr uint32_t PAGE_SIZE = 4096;

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

            uint32_t *entry_ptr = reinterpret_cast<uint32_t *>(&sector_buffer[entry_offset]);
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
        const uint8_t *source_buffer = static_cast<const uint8_t *>(buffer);
        uint32_t total_bytes_written = 0;

        if (m_first_cluster == 0) {
            uint32_t free_cluster = 0;
            
            for (uint32_t c = 2; c < VFS::FAT32::CLUSTER_MAX; ++c) { 
                if (m_fs->read_FAT_entry(c) == VFS::FAT32::CLUSTER_FREE) {
                    free_cluster = c;
                    break;
                }
            }
            if (free_cluster == 0) return -1;

            m_fs->write_FAT_entry(free_cluster, VFS::FAT32::CLUSTER_EOF_MAX); 
            m_first_cluster = free_cluster;
            m_is_dirty = true;
        }

        uint32_t current_cluster = m_first_cluster;
        uint32_t clusters_to_skip = offset / cluster_size;

        for (uint32_t i = 0; i < clusters_to_skip; ++i) {
            uint32_t next_cluster = m_fs->read_FAT_entry(current_cluster);
            
            if (next_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) { 
                uint32_t free_cluster = 0;
                for (uint32_t c = 2; c < VFS::FAT32::CLUSTER_MAX; ++c) {
                    if (m_fs->read_FAT_entry(c) == VFS::FAT32::CLUSTER_FREE) {
                        free_cluster = c;
                        break;
                    }
                }
                if (free_cluster == 0) return total_bytes_written;

                m_fs->write_FAT_entry(current_cluster, free_cluster);
                m_fs->write_FAT_entry(free_cluster, VFS::FAT32::CLUSTER_EOF_MAX);
                current_cluster = free_cluster;
            } else {
                current_cluster = next_cluster;
            }
        }

        uint8_t *sector_buffer = static_cast<uint8_t *>(
            PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE)
        );

        while (total_bytes_written < size && current_cluster < VFS::FAT32::CLUSTER_EOF_MIN) {
            uint32_t bytes_needed = size - total_bytes_written;
            uint32_t start_cluster_offset = (offset + total_bytes_written) % cluster_size;
            uint32_t current_run_bytes = cluster_size - start_cluster_offset;

            uint32_t start_cluster = current_cluster;
            uint32_t last_cluster_in_run = current_cluster;

            while (current_run_bytes < bytes_needed) {
                uint32_t next_cluster = m_fs->read_FAT_entry(last_cluster_in_run);
                
                if (next_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) {
                    uint32_t free_cluster = 0;
                    for (uint32_t c = 2; c < VFS::FAT32::CLUSTER_MAX; ++c) {
                        if (m_fs->read_FAT_entry(c) == VFS::FAT32::CLUSTER_FREE) {
                            free_cluster = c;
                            break;
                        }
                    }
                    if (free_cluster == 0) {
                        break;
                    }

                    m_fs->write_FAT_entry(last_cluster_in_run, free_cluster);
                    m_fs->write_FAT_entry(free_cluster, VFS::FAT32::CLUSTER_EOF_MAX);
                    next_cluster = free_cluster;
                }

                if (next_cluster != last_cluster_in_run + 1) {
                    break;
                }

                last_cluster_in_run = next_cluster;
                current_run_bytes += cluster_size;
            }

            uint32_t run_total_bytes = (bytes_needed < current_run_bytes) ? bytes_needed : current_run_bytes;
            uint32_t run_start_lba = m_fs->cluster_to_sector(start_cluster);
            uint32_t run_start_byte = start_cluster_offset;

            uint32_t run_bytes_written = 0;
            while (run_bytes_written < run_total_bytes) {
                uint32_t bytes_remaining = run_total_bytes - run_bytes_written;
                uint32_t current_byte_pos = run_start_byte + run_bytes_written;
                uint32_t current_sector_lba = run_start_lba + (current_byte_pos / bytes_per_sector);
                uint32_t sector_offset = current_byte_pos % bytes_per_sector;

                if (sector_offset != 0) {
                    uint32_t head_chunk = bytes_per_sector - sector_offset;
                    if (head_chunk > bytes_remaining) {
                        head_chunk = bytes_remaining;
                    }

                    m_fs->read_sectors(current_sector_lba, sector_buffer, 1);
                    memcpy(sector_buffer + sector_offset, source_buffer + total_bytes_written, head_chunk);

                    if (m_fs->write_sectors(current_sector_lba, sector_buffer, 1) < 1) {
                        break;
                    }

                    total_bytes_written += head_chunk;
                    run_bytes_written += head_chunk;
                } else if (bytes_remaining >= bytes_per_sector) {
                    uint32_t full_sectors = bytes_remaining / bytes_per_sector;
                    int written_count = m_fs->write_sectors(current_sector_lba, source_buffer + total_bytes_written, full_sectors);
                    if (written_count <= 0) {
                        break;
                    }

                    uint32_t written_bytes = static_cast<uint32_t>(written_count) * bytes_per_sector;
                    total_bytes_written += written_bytes;
                    run_bytes_written += written_bytes;
                } else {
                    m_fs->read_sectors(current_sector_lba, sector_buffer, 1);
                    memcpy(sector_buffer, source_buffer + total_bytes_written, bytes_remaining);

                    if (m_fs->write_sectors(current_sector_lba, sector_buffer, 1) < 1) {
                        break;
                    }

                    total_bytes_written += bytes_remaining;
                    run_bytes_written += bytes_remaining;
                }
            }

            if (run_bytes_written == 0) {
                break;
            }

            current_cluster = m_fs->read_FAT_entry(last_cluster_in_run);
        }

        PMEM::free_page(sector_buffer);

        if (offset + total_bytes_written > m_size) {
            m_size = offset + total_bytes_written;
            m_is_dirty = true;
        }

        if (m_is_dirty && m_dir_cluster != 0) {
            uint8_t *dir_sector_buffer = static_cast<uint8_t *>(
                PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE)
            );

            uint32_t dir_sector_lba = m_fs->cluster_to_sector(m_dir_cluster) + (m_dir_offset / bytes_per_sector);
            uint32_t entry_offset_in_sector = m_dir_offset % bytes_per_sector;

            if (m_fs->read_sectors(dir_sector_lba, dir_sector_buffer, 1) >= 1) {
                DirectoryEntry *entry = reinterpret_cast<DirectoryEntry *>(&dir_sector_buffer[entry_offset_in_sector]);
                
                entry->file_size = static_cast<uint32_t>(m_size);
                entry->cluster_high = static_cast<uint16_t>((m_first_cluster >> 16) & 0xFFFF); 
                entry->cluster_low = static_cast<uint16_t>(m_first_cluster & 0xFFFF);        

                m_fs->write_sectors(dir_sector_lba, dir_sector_buffer, 1);
                m_is_dirty = false;
            }

            PMEM::free_page(dir_sector_buffer);
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

        uint32_t cluster_size = m_fs->get_cluster_size();
        uint32_t sectors_per_cluster = cluster_size / m_fs->get_bytes_per_sector();
        uint32_t current_cluster = m_first_cluster;

        size_t pages_needed = (cluster_size + PAGE_SIZE - 1) / PAGE_SIZE;
        uint8_t *cluster_buffer = static_cast<uint8_t *>(
            PMEM::alloc_pages(pages_needed, VMM::PTE_PRESENT | VMM::PTE_WRITABLE)
        );

        while (true) {
            uint32_t cluster_base_sector = m_fs->cluster_to_sector(current_cluster);

            m_fs->read_sectors(cluster_base_sector, cluster_buffer, sectors_per_cluster);

            uint32_t total_entries_in_cluster = cluster_size / sizeof(DirectoryEntry);
            DirectoryEntry *entry_array = reinterpret_cast<DirectoryEntry *>(cluster_buffer);

            for (uint32_t entry_index = START_INDEX; entry_index < total_entries_in_cluster; ++entry_index) {
                DirectoryEntry &entry = entry_array[entry_index];
                uint8_t initial_byte = static_cast<uint8_t>(entry.name[FIRST_BYTE_INDEX]);

                if (initial_byte == DIR_ENTRY_FREE || initial_byte == DIR_ENTRY_FREE_ONWARD) {
                    memcpy(entry.name, target_83_name, FAT_83_NAME_SIZE);
                    entry.attr = (type == VFS::FileType::Directory) ? ATTR_DIRECTORY : ATTR_ARCHIVE;
                    entry.file_size = INITIAL_SIZE;
                    entry.cluster_high = INITIAL_CLUSTER_HI;
                    entry.cluster_low = INITIAL_CLUSTER_LO;

                    m_fs->write_sectors(cluster_base_sector, cluster_buffer, sectors_per_cluster);
                    PMEM::free_pages(cluster_buffer, pages_needed);

                    uint32_t total_dir_offset = entry_index * sizeof(DirectoryEntry);

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
                    PMEM::alloc_pages(pages_needed, VMM::PTE_PRESENT | VMM::PTE_WRITABLE)
                );
                memset(zero_buffer, 0, cluster_size);

                uint32_t new_cluster_base_sector = m_fs->cluster_to_sector(free_cluster);
                m_fs->write_sectors(new_cluster_base_sector, zero_buffer, sectors_per_cluster);
                
                PMEM::free_pages(zero_buffer, pages_needed);

                current_cluster = free_cluster;
            } else {
                current_cluster = next_cluster;
            }
        }

        PMEM::free_pages(cluster_buffer, pages_needed);
        return nullptr;
    }

    VFS::VNode *FAT32FileSystem::get_root_node() {
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

        if (safe_read_size == 0) {
            return 0;
        }

        uint32_t cluster_size = m_fs->get_cluster_size();
        uint32_t bytes_per_sector = m_fs->get_bytes_per_sector();

        uint32_t current_cluster = m_first_cluster;
        uint32_t clusters_to_skip = offset / cluster_size;

        for (uint32_t i = 0; i < clusters_to_skip; ++i) {
            if (current_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) {
                return 0;
            }
            current_cluster = m_fs->read_FAT_entry(current_cluster);
        }

        if (current_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) {
            return 0;
        }

        uint8_t *destination_buffer = static_cast<uint8_t *>(buffer);
        uint32_t total_bytes_read = 0;

        uint8_t *sector_buffer = static_cast<uint8_t *>(
            PMEM::alloc_page(VMM::PTE_PRESENT | VMM::PTE_WRITABLE)
        );

        while (total_bytes_read < safe_read_size && current_cluster < VFS::FAT32::CLUSTER_EOF_MIN) {
            uint32_t bytes_needed = safe_read_size - total_bytes_read;
            uint32_t start_cluster_offset = (offset + total_bytes_read) % cluster_size;
            uint32_t current_run_bytes = cluster_size - start_cluster_offset;

            uint32_t start_cluster = current_cluster;
            uint32_t last_cluster_in_run = current_cluster;

            while (current_run_bytes < bytes_needed) {
                uint32_t next_cluster = m_fs->read_FAT_entry(last_cluster_in_run);
                if (next_cluster != last_cluster_in_run + 1 || next_cluster >= VFS::FAT32::CLUSTER_EOF_MIN) {
                    break;
                }
                last_cluster_in_run = next_cluster;
                current_run_bytes += cluster_size;
            }

            uint32_t run_total_bytes = (bytes_needed < current_run_bytes) ? bytes_needed : current_run_bytes;
            uint32_t run_start_lba = m_fs->cluster_to_sector(start_cluster);
            uint32_t run_start_byte = start_cluster_offset;

            uint32_t run_bytes_read = 0;
            while (run_bytes_read < run_total_bytes) {
                uint32_t bytes_remaining = run_total_bytes - run_bytes_read;
                uint32_t current_byte_pos = run_start_byte + run_bytes_read;
                uint32_t current_sector_lba = run_start_lba + (current_byte_pos / bytes_per_sector);
                uint32_t sector_offset = current_byte_pos % bytes_per_sector;

                if (sector_offset != 0) {
                    uint32_t head_chunk = bytes_per_sector - sector_offset;
                    if (head_chunk > bytes_remaining) {
                        head_chunk = bytes_remaining;
                    }

                    if (m_fs->read_sectors(current_sector_lba, sector_buffer, 1) < 1) {
                        break;
                    }

                    memcpy(destination_buffer + total_bytes_read, sector_buffer + sector_offset, head_chunk);
                    total_bytes_read += head_chunk;
                    run_bytes_read += head_chunk;
                } else if (bytes_remaining >= bytes_per_sector) {
                    uint32_t full_sectors = bytes_remaining / bytes_per_sector;
                    int read_count = m_fs->read_sectors(current_sector_lba, destination_buffer + total_bytes_read, full_sectors);
                    if (read_count <= 0) {
                        break;
                    }

                    uint32_t read_bytes = static_cast<uint32_t>(read_count) * bytes_per_sector;
                    total_bytes_read += read_bytes;
                    run_bytes_read += read_bytes;
                } else {
                    if (m_fs->read_sectors(current_sector_lba, sector_buffer, 1) < 1) {
                        break;
                    }

                    memcpy(destination_buffer + total_bytes_read, sector_buffer, bytes_remaining);
                    total_bytes_read += bytes_remaining;
                    run_bytes_read += bytes_remaining;
                }
            }

            if (run_bytes_read == 0) {
                break;
            }

            current_cluster = m_fs->read_FAT_entry(last_cluster_in_run);
        }

        PMEM::free_page(sector_buffer);
        return total_bytes_read;
    }

    HAL::DISK::Disk *FAT32FileSystem::get_disk_device() const {
        return m_disk_device;
    }

    VFS::VNode *FAT32VNode::lookup(const char *name) {
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

        uint32_t cluster_size = m_fs->get_cluster_size();
        uint32_t sectors_per_cluster = cluster_size / m_fs->get_bytes_per_sector();
        uint32_t current_cluster = m_first_cluster;

        size_t pages_needed = (cluster_size + PAGE_SIZE - 1) / PAGE_SIZE;
        uint8_t *cluster_buffer = static_cast<uint8_t *>(
            PMEM::alloc_pages(pages_needed, VMM::PTE_PRESENT | VMM::PTE_WRITABLE)
        );

        while (current_cluster < VFS::FAT32::CLUSTER_EOF_MIN) {
            uint32_t cluster_base_sector = m_fs->cluster_to_sector(current_cluster);

            m_fs->read_sectors(cluster_base_sector, cluster_buffer, sectors_per_cluster);

            uint32_t total_entries_in_cluster = cluster_size / sizeof(DirectoryEntry);
            DirectoryEntry *entry_array = reinterpret_cast<DirectoryEntry *>(cluster_buffer);

            for (uint32_t entry_index = 0; entry_index < total_entries_in_cluster; ++entry_index) {
                DirectoryEntry &entry = entry_array[entry_index];
                uint8_t initial_name_byte = static_cast<uint8_t>(entry.name[0]);

                if (initial_name_byte == VFS::FAT32::DIR_ENTRY_FREE_ONWARD) { 
                    PMEM::free_pages(cluster_buffer, pages_needed);
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

                    uint32_t intra_cluster_byte_offset = entry_index * sizeof(DirectoryEntry);

                    VFS::VNode *discovered_node = new FAT32VNode(
                        m_fs,
                        calculated_type,
                        entry.file_size,
                        entry_target_cluster,
                        current_cluster,
                        intra_cluster_byte_offset
                    );

                    PMEM::free_pages(cluster_buffer, pages_needed);
                    return discovered_node;
                }
            }

            current_cluster = m_fs->read_FAT_entry(current_cluster);
        }

        PMEM::free_pages(cluster_buffer, pages_needed);
        return nullptr;
    }

    FAT32VNode::FAT32VNode(
        FAT32FileSystem *fs, 
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

    int FAT32FileSystem::read_sectors(uint64_t sector, void *buffer, uint32_t count) {
        return m_disk_device->dev->read(sector, count, buffer);
    }

    int FAT32FileSystem::write_sectors(uint64_t sector, const void *buffer, uint32_t count) {
        return m_disk_device->dev->write(sector, count, const_cast<void *>(buffer));
    }
}