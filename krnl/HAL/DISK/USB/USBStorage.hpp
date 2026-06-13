#pragma once
#include <HAL/DISK/Disk.hpp>
#include <HAL/PCI/xHCI/xHCIDriver.hpp>
#include <HAL/PCI/xHCI/xHCI.hpp>

namespace HAL::DISK::USB {
    struct CBW {
        uint32_t signature;
        uint32_t tag;
        uint32_t data_transfer_len;
        uint8_t  flags;
        uint8_t  lun;
        uint8_t  command_length;
        uint8_t  scsi_command[16];
    } __attribute__((packed));

    struct CSW {
        uint32_t signature;
        uint32_t tag;
        uint32_t data_residue;
        uint8_t  status;
    } __attribute__((packed));

    class USBStorage;

    class xHCIDD : public DiskDevice {
    private:
        USBStorage *usbptr;
    public:
        virtual int read(uint64_t sector, uint32_t count, void *buffer) override;
        virtual int write(uint64_t sector, uint32_t count, void* buffer) override;

        friend USBStorage;
    };

    class USBStorage : public PCI::xHCIDriver {
    public:
        virtual void initialize(PCI::xHCI* _ctrl, uint8_t _slot, void *endpoints, int ep_count) override;
        virtual void on_int(uint32_t bytes_transferred, uint32_t endpoint_id, uint64_t param_event) override;
        virtual void start() override;

        uint8_t bulk_in_ep;
        uint8_t bulk_out_ep;

        bool initialized = false;

    private:
        PCI::xHCI *controller;
        xHCIDD *fs_usbptr;
        uint8_t slot_id;

        int read_sectors(uint32_t lba, uint16_t count, void *buffer);
        void write_sectors(uint32_t lba, uint16_t count, void *buffer);

        enum MSCState {
            STATE_IDLE,
            STATE_WAIT_CBW,
            STATE_WAIT_DATA,
            STATE_WAIT_CSW
        };

        volatile MSCState state = STATE_IDLE;

        uint64_t current_data_phys;
        uint32_t current_data_len;

        CBW* current_cbw;
        CSW* current_csw;
        uint64_t cbw_phys;
        uint64_t csw_phys;

        uint8_t* bounce_virt;
        uint64_t bounce_phys;

        volatile bool io_pending = false;

        static constexpr uint32_t VALID_STATUS = 0;
        static constexpr uint32_t BULK_BUFF_SIZE = 13;

        static constexpr uint8_t EP_DIR_MASK        = 0x80;
        static constexpr uint8_t EP_NUM_MASK        = 0x0F;
        static constexpr uint8_t EP_ATTR_TYPE_MASK  = 0x03;
        static constexpr uint8_t EP_ATTR_BULK       = 0x02;

        static constexpr uint32_t CBW_SIGNATURE     = 0x43425355;
        static constexpr uint32_t CSW_SIGNATURE     = 0x53425355;
        static constexpr uint32_t CBW_SIZE          = 31;
        static constexpr uint32_t CSW_SIZE          = 13;
        static constexpr uint8_t  CBW_FLAG_IN       = 0x80;
        static constexpr uint8_t  CBW_FLAG_OUT      = 0x00;
        static constexpr uint8_t  CBW_MAX_CDB_LEN   = 16;

        static constexpr uint8_t  SCSI_CMD_READ10   = 0x28;
        static constexpr uint8_t  SCSI_CMD_LEN10    = 10;
        static constexpr uint32_t SECTOR_SIZE       = 512;

        static constexpr uint8_t  CSW_STATUS_PASSED = 0x00;
        static constexpr uint8_t  CSW_STATUS_FAILED = 0x01;
        static constexpr uint8_t  CSW_STATUS_PHASE  = 0x02;

        friend xHCIDD;
    };
}