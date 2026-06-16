#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/CORE/Core.hpp>
#include <HAL/DISK/Disk.hpp>
#include <Library/debug.hpp>
#include <Library/regs.h>
#include <VFS/FAT32/FAT32.hpp>
#include <Services/TTY/TTY.hpp>
#include <limine.h>

static constinit volatile limine_executable_file_request exec_file_request = {
    .id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

namespace HAL::DISK {
    Disk *disks[MAX_DISKS]{nullptr};
    volatile char root_disk_id{};

    bool a_kmem_lock = false;
    uint64_t cur_rflags = 0;
    uint64_t core_id_holder = -1;
    int reentrancy = 0;
    void aquire_lock() {
        if (core_id_holder == HAL::CORE::get_thread_data()->current_task->get_pid()) {
            ++reentrancy;
            return;
        }
        
        uint64_t rflags = 0;
        asm volatile("pushfq; pop %0" : "=r"(rflags));
        asm volatile("cli");
        while (__atomic_test_and_set(&a_kmem_lock, __ATOMIC_ACQUIRE)) {
            asm volatile("pause");
        }

        cur_rflags = rflags;
        core_id_holder = HAL::CORE::get_thread_data()->current_task->get_pid();

        return;
    }

    void release_lock() {
        if (reentrancy > 0) {
            --reentrancy;
            return;
        }

        restore_rflags(cur_rflags);
        cur_rflags = 0;

        __atomic_clear(&a_kmem_lock, __ATOMIC_RELEASE);
        return;
    }

    Disk::Disk(char letter, DiskDevice *dev) : drv_ltr(letter), dev(dev) {}

    bool Disk::initializefs() {
        aquire_lock();
        Debug::krnl_print("DISK", Debug::LOG_INFO, "Begin filesystem init!");
        if (initialized) {
            Debug::krnl_print("DISK", Debug::LOG_WARN, "Reinitializing disk!");
        }

        auto *fs = new VFS::FAT32::FAT32FileSystem(this);

        Debug::krnl_print("DISK", Debug::LOG_INFO, "Created FS instance");

        if (!fs->initialize()) {
            Debug::krnl_print("DISK", Debug::LOG_WARN, "Failed to initialize FS (incorrect format?)");
            delete fs;
            release_lock();
            return false;
        }

        Debug::krnl_print("DISK", Debug::LOG_INFO, "Succesfully initialized FS");
        
        if (TTY::conhosts[TTY::active_host]->_ltrdrive == '?') {
            TTY::conhosts[TTY::active_host]->early_drive_swap(drv_ltr);
        }

        rootnode = fs->get_root_node();

        initialized = true;
        release_lock();
        return true;
    }

    bool Disk::is_initialized() {
        return initialized;
    }

    Disk *Disk::CreateDisk(DiskDevice *dev, char letter) {
        aquire_lock();
        if (!IsCapital(letter)) {
            letter = GetValidDriveLabel();
            release_lock();
            if (!letter) return nullptr;
        }

        if (disks[letter - 'A']) {
            letter = GetValidDriveLabel();
            release_lock();
            if (!letter) return nullptr;
        }

        Disk *n_disk = new Disk(letter, dev);
        disks[letter - 'A'] = n_disk;

        char disk_name[] = {letter, ':', '/', 0};
        Debug::krnl_print("DISK", Debug::LOG_INFO, "New disk successfully configured (Drive: %s). Remember to initialize the FS!", disk_name);
        release_lock();
        return n_disk;
    }

    bool Disk::found_bootdisk{false};

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

    static bool is_uuid_nonzero(const limine_uuid& uuid) {
        const auto* half_words = reinterpret_cast<const uint64_t*>(&uuid);
        return (half_words[0] != 0) || (half_words[1] != 0);
    }

    UUID convert_limine_uuid(const limine_uuid &l_uuid) {
        UUID new_uuid{};
        
        const auto* src = reinterpret_cast<const uint8_t*>(&l_uuid);
        
        for (size_t i = 0; i < SIZE_OF_UUID; ++i) {
            new_uuid.bytes[i] = src[i];
        }

        return new_uuid;
    }

    static bool operator==(UUID a, UUID &b) {
        for (auto i{0uz}; i < SIZE_OF_UUID; i++) {
            if (a.bytes[i] != b.bytes[i]) return false;
        }

        return true;
    }

    bool bootTypeWasGPT = false;
    uint32_t mbr_id{};
    UUID gpt_uuid{};
    void InitDiskData() {
        if (exec_file_request.response == NULL || exec_file_request.response->executable_file == NULL) {
            panic(PanicReasons::HAL_FAILED_INITIALIZATION);
        }

        limine_file *boot_file = exec_file_request.response->executable_file;
        
        if (is_uuid_nonzero(boot_file->gpt_disk_uuid)) {
            bootTypeWasGPT = true;
            gpt_uuid = convert_limine_uuid(boot_file->gpt_disk_uuid);
        }

        mbr_id = boot_file->mbr_disk_id;
    }

    bool CheckIfCoreDisk(UUID gpt) {
        if (gpt == gpt_uuid) return true;
        return false;
    }

    bool CheckIfCoreDisk(uint32_t mbr) {
        if (mbr == mbr_id) return true;
        return false;
    }

    VFS::VNode *GetRootOfDrive(char drive) {
        if (!IsValidDisk(drive))
            return nullptr;

        Disk *d = disks[drive - 'A'];
        return d->rootnode;
    }

    // FIX: Magic Numbers here!
    uint64_t find_fat32_lba(DiskDevice* dev) {
        aquire_lock();
        uint8_t *sector_buffer = (uint8_t *)MEM::PMEM::alloc_page(MEM::VMM::PTE_PRESENT | MEM::VMM::PTE_WRITABLE);
        dev->read(0, 1, sector_buffer);

        if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
            Debug::krnl_print("PART", Debug::LOG_ERROR, "Invalid MBR signature!");
            release_lock();
            return 0; 
        }

        auto* mbr_entries = reinterpret_cast<MBRPartitionEntry*>(sector_buffer + 0x1BE);

        bool is_gpt = false;

        for (int i = 0; i < 4; i++) {
            if (mbr_entries[i].type == 0x0B || mbr_entries[i].type == 0x0C) {
                Debug::krnl_print("PART", Debug::LOG_INFO, "Found MBR FAT32 Partition at LBA %x", mbr_entries[i].lba_start);
                release_lock();
                return mbr_entries[i].lba_start;
            }
            if (mbr_entries[i].type == 0xEE) {
                is_gpt = true;
                break;
            }
        }

        if (is_gpt) {
            Debug::krnl_print("PART", Debug::LOG_INFO, "Protective MBR found. Parsing GPT...");
            
            dev->read(1, 1, sector_buffer);
            auto* gpt_header = reinterpret_cast<GPTHeader*>(sector_buffer);

            if (gpt_header->signature != 0x5452415020494645ULL) {
                Debug::krnl_print("PART", Debug::LOG_ERROR, "Invalid GPT Signature!");
                release_lock();
                return 0;
            }

            uint64_t entry_lba = gpt_header->partition_entry_lba;
            uint32_t num_entries = gpt_header->num_partition_entries;
            uint32_t entry_size = gpt_header->sizeof_partition_entry;

            dev->read(entry_lba, 1, sector_buffer);

            int entries_in_sector = 512 / entry_size;
            for (uint32_t i = 0; i < (uint32_t)entries_in_sector && i < num_entries; i++) {
                auto* entry = reinterpret_cast<GPTPartitionEntry*>(sector_buffer + (i * entry_size));

                bool is_empty = true;
                for(int j=0; j<16; j++) if(entry->partition_type_guid[j] != 0) is_empty = false;
                if (is_empty) continue;

                Debug::krnl_print("PART", Debug::LOG_INFO, "Found GPT Partition at LBA %x", entry->starting_lba);
                release_lock();
                return entry->starting_lba;
            }

            Debug::krnl_print("PART", Debug::LOG_ERROR, "No valid partition found!");
            release_lock();
            return 0;
        }

        release_lock();

        return 0;
    }

    bool IsValidDisk(char ch) {
        if (disks[ch - 'A']) return true;
        return false;
    }

    Disk *GetDisk(char ch) {
        if (!IsValidDisk(ch)) return nullptr;
        return disks[ch - 'A'];
    }
}