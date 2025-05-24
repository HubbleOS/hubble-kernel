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

if [ ! -f "$EFI_SOURCE" ]; then
	echo "❌ File not found: $EFI_SOURCE"
	exit 1
fi

echo "Creating $IMG_NAME image of 1.44MB..."
dd if=/dev/zero of="$IMG_SOURCE" bs=1k count=1440

echo "Formatting FAT12..."
mformat -i "$IMG_SOURCE" -f 1440 ::

echo "Creating structure EFI..."
mmd -i "$IMG_SOURCE" ::/EFI
mmd -i "$IMG_SOURCE" ::/EFI/BOOT

echo "Copying files..."
mcopy -i "$IMG_SOURCE" "$EFI_SOURCE" ::/EFI/BOOT
mcopy -i "$IMG_SOURCE" "$ISO_DIR/kernel.bin" ::

echo "✅ UEFI FAT-image created: $IMG_SOURCE"
