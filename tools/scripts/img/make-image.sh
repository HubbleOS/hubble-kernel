#!/bin/bash
set -e

WORKDIR="$(pwd)"
ARCH="x86"
OUTPUT_DIR="$WORKDIR/out"
OUTPUT_DIR="$OUTPUT_DIR/$ARCH"
ISO_DIR="$OUTPUT_DIR/iso"

IMG_NAME="fat.img"
IMG_SOURCE="$OUTPUT_DIR/$IMG_NAME"
EFI_SOURCE="$ISO_DIR/EFI/BOOT/BOOTX64.EFI"

IMG_SIZE_MB=64
MOUNT_DIR="$OUTPUT_DIR/mnt"

# check efi
if [ ! -f "$EFI_SOURCE" ]; then
    echo "❌ File not found: $EFI_SOURCE"
    exit 1
fi
# check if already mounted
if [ -d "$MOUNT_DIR" ]; then
    echo "❌ Directory already mounted: $MOUNT_DIR"
    exit 1
fi

echo "[+] Creating FAT32 image of ${IMG_SIZE_MB}MB..."
dd if=/dev/zero of="$IMG_SOURCE" bs=1M count=$IMG_SIZE_MB

echo "[+] Formatting as FAT32..."
mkfs.vfat -F 32 "$IMG_SOURCE"

echo "[+] Mounting..."
mkdir -p "$MOUNT_DIR"
sudo mount "$IMG_SOURCE" "$MOUNT_DIR"

echo "[+] Creating EFI structure..."
sudo mkdir -p "$MOUNT_DIR/EFI/BOOT"

echo "[+] Copying files..."
sudo cp "$EFI_SOURCE" "$MOUNT_DIR/EFI/BOOT/"
sudo cp "$ISO_DIR/kernel.bin" "$MOUNT_DIR/"
sudo cp "$ISO_DIR/ramdisk.img" "$MOUNT_DIR/"

echo "[+] Syncing..."
sync

echo "[+] Unmounting..."
sudo umount "$MOUNT_DIR"
rmdir "$MOUNT_DIR"

echo "[+] FAT32 UEFI image created: $IMG_SOURCE"
