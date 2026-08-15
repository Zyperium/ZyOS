echo "Warning: This will destroy /dev/sda"
sudo dd if=disk_uefi.img of=/dev/sda bs=4M status=progress conv=fdatasync