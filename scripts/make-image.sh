#!/bin/bash
set -e

WORKDIR="$(pwd)"

OUTPUT_DIR="$WORKDIR/output"
IMG_NAME="fat.img"
IMG_SOURCE="$OUTPUT_DIR/$IMG_NAME"
EFI_SOURCE="$WORKDIR/iso/x86/EFI/BOOT/BOOTX64.EFI"

if [ ! -f "$EFI_SOURCE" ]; then
	echo "❌ File not found: $EFI_SOURCE"
	exit 1
fi

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

echo "Creating $IMG_NAME image of 1.44MB..."
dd if=/dev/zero of="$IMG_SOURCE" bs=1k count=1440

echo "Formatting FAT12..."
mformat -i "$IMG_SOURCE" -f 1440 ::

echo "Creating structure EFI..."
mmd -i "$IMG_SOURCE" ::/EFI
mmd -i "$IMG_SOURCE" ::/EFI/BOOT

echo "Copying files..."
mcopy -i "$IMG_SOURCE" "$EFI_SOURCE" ::/EFI/BOOT
mcopy -i "$IMG_SOURCE" "$WORKDIR/iso/x86/kernel.bin" ::

echo "✅ UEFI FAT-image created: $IMG_SOURCE"
