# ZyOS master makefile
KERNEL_NAME = zyoskrnl.elf
DISK_BIOS   = disk_bios.img
DISK_UEFI   = disk_uefi.img

BUILD_DIR   := build
ISO_DIR     := iso_root
LIMINE_DIR  := limine
ASSETS_DIR  := assets

SUBDIRS     := drivers krnl programs

export KERNEL_NAME BUILD_DIR

.PHONY: all bios uefi clean run run-uefi dbg check-env disk $(SUBDIRS)

all: bios

bios: $(SUBDIRS) $(DISK_BIOS)
uefi: $(SUBDIRS) $(DISK_UEFI)

disk:
	@$(MAKE) -B $(DISK_BIOS)
	@$(MAKE) -B $(DISK_UEFI)

$(SUBDIRS):
	@echo " Entering Subdirectory: $@"
	@$(MAKE) -C $@

define DEPLOY_CONTENT
	mcopy -i $(1)@@1M $(ISO_DIR)/limine.conf ::/limine.conf
	@find $(ASSETS_DIR) -type f \( -name "Thumbs.db" -o -name "desktop.ini" \) -delete

	@if [ -d $(ASSETS_DIR) ] && [ "$$(ls -A $(ASSETS_DIR) 2>/dev/null)" ]; then \
		mcopy -s -i $(1)@@1M $(ASSETS_DIR)/* ::/; \
	fi

	@if ! mdir -i $(1)@@1M ::/SYSTEM >/dev/null 2>&1; then mmd -i $(1)@@1M ::/SYSTEM; fi
	@if ! mdir -i $(1)@@1M ::/SYSTEM/DRIVERS >/dev/null 2>&1; then mmd -i $(1)@@1M ::/SYSTEM/DRIVERS; fi
	@if ! mdir -i $(1)@@1M ::/USER >/dev/null 2>&1; then mmd -i $(1)@@1M ::/USER; fi

	@if ls drivers/build/*.KMO 1> /dev/null 2>&1; then \
		mcopy -i $(1)@@1M drivers/build/*.KMO ::/SYSTEM/DRIVERS/; \
	fi
	@if ls programs/build/*.zyx 1> /dev/null 2>&1; then \
		mcopy -i $(1)@@1M programs/build/*.zyx ::/USER/; \
	fi

	@nm -n krnl/build/$(KERNEL_NAME) > krnl/build/krnl.map 2>/dev/null || true
	@if [ -f krnl/build/krnl.map ]; then mcopy -i $(1)@@1M krnl/build/krnl.map ::/SYSTEM/KRNL.MAP; fi
	mcopy -i $(1)@@1M krnl/build/$(KERNEL_NAME) ::/SYSTEM/$(KERNEL_NAME)
	@python3 toolchain/gen_sym_map.py krnl/build/krnl.map || true
	@if [ -f krnl/build/khash.map ]; then mcopy -i $(1)@@1M krnl/build/khash.map ::/SYSTEM/KHASH.MAP; fi
endef

$(DISK_BIOS):
	@echo "Creating BIOS Disk Image..."
	dd if=/dev/zero of=$(DISK_BIOS) bs=1M count=64
	sgdisk $(DISK_BIOS) -n 1:2048:0 -t 1:ef00 -n 2:34:2047 -t 2:ef02
	mformat -i $(DISK_BIOS)@@1M -F ::
	mmd -i $(DISK_BIOS)@@1M ::/EFI
	mmd -i $(DISK_BIOS)@@1M ::/EFI/BOOT
	mcopy -i $(DISK_BIOS)@@1M $(LIMINE_DIR)/limine-bios.sys ::/limine-bios.sys
	$(call DEPLOY_CONTENT,$(DISK_BIOS))
	$(LIMINE_DIR)/limine bios-install $(DISK_BIOS)
	@echo "Legacy BIOS Image Built: $(DISK_BIOS)"

$(DISK_UEFI):
	@echo "Creating UEFI Disk Image..."
	dd if=/dev/zero of=$(DISK_UEFI) bs=1M count=64
	sgdisk $(DISK_UEFI) -n 1:2048:0 -t 1:ef00
	mformat -i $(DISK_UEFI)@@1M -F -v "ZYOS_ESP" ::
	mmd -i $(DISK_UEFI)@@1M ::/EFI
	mmd -i $(DISK_UEFI)@@1M ::/EFI/BOOT
	mcopy -i $(DISK_UEFI)@@1M $(LIMINE_DIR)/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	$(call DEPLOY_CONTENT,$(DISK_UEFI))
	@echo "UEFI Disk Image Built: $(DISK_UEFI)"

run: $(DISK_BIOS)
ifeq ($(OS),Windows_NT)
	qemu-system-x86_64 -cpu qemu64 -m 512M -machine pc,hpet=off -accel whpx,kernel-irqchip=off \
		-drive file=$(DISK_BIOS),id=bootdisk,format=raw,if=none \
		-device ich9-ahci,id=ahci -device ide-hd,drive=bootdisk,bus=ahci.0 \
		-display sdl -vga std -device qemu-xhci,id=xhci \
		-device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0 \
		-rtc base=localtime -d int,cpu_reset -no-reboot -no-shutdown -D qemu.log
else
	qemu-system-x86_64 -cpu host -m 512M -accel kvm -machine q35,acpi=on \
        -drive file=disk_bios.img,id=usbdisk,format=raw,if=none \
        -device qemu-xhci,id=xhci \
        -device usb-storage,bus=xhci.0,drive=usbdisk,bootindex=1 \
        -display sdl -vga std \
        -rtc base=localtime \
        -d int,cpu_reset \
        -no-reboot -no-shutdown -D qemu.log \
        -debugcon stdio -smp 1
endif
#-accel kvm

OVMF_URL = https://github.com/clearlinux/common/raw/master/OVMF.fd

run-uefi: $(DISK_UEFI)
	@if [ ! -f OVMF.fd ]; then wget $(OVMF_URL); fi
	qemu-system-x86_64 -cpu host -m 512M -accel kvm -machine pc \
		-bios OVMF.fd \
		-drive file=$(DISK_UEFI),format=raw,if=none,id=usbdisk \
		-device qemu-xhci,id=xhci -device usb-storage,bus=xhci.0,drive=usbdisk,bootindex=0 \
		-device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0 \
		-debugcon stdio -vga std -rtc base=localtime

dbg: $(DISK_BIOS)
	sudo qemu-system-x86_64 -cpu max -m 512M \
        -machine q35,kernel-irqchip=off \
        -device pcie-root-port,id=pcie.1,bus=pcie.0,slot=1,chassis=1 \
        -drive file=disk_bios.img,id=satadisk,format=raw,if=none \
        -device ide-hd,bus=ide.0,unit=0,drive=satadisk,bootindex=1 \
        -device qemu-xhci,id=xhci,bus=pcie.1,addr=00.0 \
        -device usb-host,vendorid=0x090c,productid=0x1000 \
        -display sdl -vga std \
        -rtc base=localtime \
        -d int,cpu_reset,guest_errors \
        -trace "usb_xhci_*" \
        -no-reboot -no-shutdown -D qemu.log \
        -debugcon stdio -smp 1

clean:
	rm -rf $(DISK_BIOS) $(DISK_UEFI) qemu.log
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

check-env:
	@echo "Current PATH inside make:"
	@echo $(PATH)
	@which x86_64-elf-gcc || echo "Cross-compiler NOT found in make's PATH"