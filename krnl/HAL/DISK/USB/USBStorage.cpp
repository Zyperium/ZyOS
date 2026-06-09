#include <HAL/DISK/USB/USBStorage.hpp>
#include <HAL/MEM/PMEM.hpp>
#include <HAL/MEM/VMM.hpp>
#include <Library/regs.h>
#include <Library/string.h>

using namespace HAL::MEM;

namespace HAL::DISK::USB {
    void xHCIDD::read(uint64_t sector, uint32_t count, void *buffer) {
        usbptr->read_sectors(sector, count, buffer);
    }

    void xHCIDD::write(uint64_t sector, uint32_t count, void* buffer) {
        usbptr->write_sectors(sector, count, buffer);
    }

    void USBStorage::initialize(PCI::xHCI* _ctrl, uint8_t _slot, void *endpoints_ptr, int ep_count) {
        controller = _ctrl;
        slot_id = _slot;

        auto* eps = reinterpret_cast<PCI::xHCI::ParsedEndpoint *>(endpoints_ptr);

        for (int i = 0; i < ep_count; i++) {
            uint8_t ep_addr = eps[i].address;
            bool is_in = (ep_addr & EP_DIR_MASK) != 0;
            uint8_t ep_num = ep_addr & EP_NUM_MASK;
            uint8_t dci = (ep_num * 2) + (is_in ? 1 : 0);

            if ((eps[i].attributes & EP_ATTR_TYPE_MASK) == EP_ATTR_BULK) {
                if (is_in) {
                    bulk_in_ep = dci;
                } else {
                    bulk_out_ep = dci;
                }
            }
        }

        current_cbw = static_cast<CBW *>(PMEM::alloc_page(VMM::PTE_CACHELESS | VMM::PTE_PRESENT | VMM::PTE_WRITABLE));
        current_csw = static_cast<CSW *>(PMEM::alloc_page(VMM::PTE_CACHELESS | VMM::PTE_PRESENT | VMM::PTE_WRITABLE));

        cbw_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)current_cbw);
        csw_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)current_csw);

        bounce_virt = static_cast<uint8_t *>(PMEM::alloc_page(VMM::PTE_CACHELESS | VMM::PTE_PRESENT | VMM::PTE_WRITABLE));
        bounce_phys = VMM::GetPhysicalAddress(read_cr3(), (uint64_t)bounce_virt);

        initialized = true;
    }

    void USBStorage::on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) {
        (void)bytes_transferred;
        (void)param_event;
        switch (state) {
            case STATE_WAIT_CBW:
                if (endpoint_id == bulk_out_ep) {
                    if (current_cbw->data_transfer_len > 0) {
                        state = STATE_WAIT_DATA;
                        uint8_t data_ep = (current_cbw->flags & EP_DIR_MASK) ? bulk_in_ep : bulk_out_ep;
                        controller->queue_bulk_transfer(slot_id, data_ep, current_data_phys, current_data_len);
                    } else {
                        state = STATE_WAIT_CSW;
                        controller->queue_bulk_transfer(slot_id, bulk_in_ep, csw_phys, CSW_SIZE);
                    }
                }
                break;

            case STATE_WAIT_DATA:
                if (endpoint_id == bulk_in_ep || endpoint_id == bulk_out_ep) {
                    state = STATE_WAIT_CSW;
                    controller->queue_bulk_transfer(slot_id, bulk_in_ep, csw_phys, BULK_BUFF_SIZE);
                }
                break;

            case STATE_WAIT_CSW:
                if (endpoint_id == bulk_in_ep) {
                    if (current_csw->signature != CSW_SIGNATURE || current_csw->status != VALID_STATUS) {
                        Debug::krnl_print("xHCI", Debug::LOG_WARN, "SCSI command failed!");
                    }
                    state = STATE_IDLE;
                    io_pending = false; 
                }
                break;

            default:
                break;
        }
    }

    void USBStorage::start() {
        Debug::krnl_print("xHCI", Debug::LOG_INFO, "Starting USB mass storage driver");
        state = STATE_IDLE;
    }
    
    void USBStorage::write_sectors(uint32_t lba, uint16_t count, void *buffer) {
        (void)lba;
        (void)count;
        (void)buffer;        

        return;
    }

    void USBStorage::read_sectors(uint32_t lba, uint16_t count, void *buffer) {
        uint8_t *dest = static_cast<uint8_t*>(buffer);
        uint32_t sectors_left = count;
        uint32_t current_lba = lba;

        const uint16_t MAX_SECTORS_PER_CHUNK = 8; 

        while (sectors_left > 0) {
            uint16_t chunk_sectors = (sectors_left > MAX_SECTORS_PER_CHUNK) ? MAX_SECTORS_PER_CHUNK : sectors_left;
            uint32_t chunk_bytes = chunk_sectors * 512;

            while (io_pending) { asm volatile("pause"); }
            io_pending = true;

            current_data_phys = bounce_phys;
            current_data_len  = chunk_bytes;

            current_cbw->signature = CBW_SIGNATURE; 
            current_cbw->tag = current_lba;
            current_cbw->data_transfer_len = chunk_bytes;
            current_cbw->flags = CBW_FLAG_IN;
            current_cbw->lun = 0;
            current_cbw->command_length = SCSI_CMD_LEN10;

            memset(current_cbw->scsi_command, 0, CBW_MAX_CDB_LEN);
            current_cbw->scsi_command[0] = SCSI_CMD_READ10;

            current_cbw->scsi_command[2] = (current_lba >> 24) & 0xFF;
            current_cbw->scsi_command[3] = (current_lba >> 16) & 0xFF;
            current_cbw->scsi_command[4] = (current_lba >> 8) & 0xFF;
            current_cbw->scsi_command[5] = current_lba & 0xFF;

            current_cbw->scsi_command[7] = (chunk_sectors >> 8) & 0xFF;
            current_cbw->scsi_command[8] = chunk_sectors & 0xFF;

            state = STATE_WAIT_CBW;
            controller->queue_bulk_transfer(slot_id, bulk_out_ep, cbw_phys, CBW_SIZE);

            while (io_pending) { asm volatile("pause"); }

            memcpy(dest, bounce_virt, chunk_bytes);

            dest += chunk_bytes;
            current_lba += chunk_sectors;
            sectors_left -= chunk_sectors;
        }
    }
}