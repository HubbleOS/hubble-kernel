#!/bin/bash
#
# build-minimal-iso.sh — Build a minimal bootable ISO for kernel development.
#
# Usage:
#   ./scripts/qemu/build-minimal-iso.sh [KERNEL_ELF] [OUTPUT_ISO]
#
# This creates a minimal ISO with the kernel, initramfs, and Limine bootloader,
# suitable for direct QEMU execution without the full hubble-os environment.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

KERNEL_ELF="${1:-$ROOT_DIR/out/kernel.elf}"
OUTPUT_ISO="${2:-$ROOT_DIR/out/kernel-dev.iso}"
INITRAMFS_IMG="${3:-$ROOT_DIR/out/initramfs.img}"

SCRIPTS_DIR="$SCRIPT_DIR"
CONFIG_DIR="$SCRIPT_DIR"
LIMINE_DATADIR="${LIMINE_DATADIR:-$(limine --print-datadir 2>/dev/null || true)}"

echo "=== Hubble Kernel Minimal ISO Builder ==="
echo "Kernel ELF:  $KERNEL_ELF"
echo "Initramfs:   $INITRAMFS_IMG"
echo "Output:      $OUTPUT_ISO"
echo ""

# Check kernel exists
if [ ! -f "$KERNEL_ELF" ]; then
    echo "ERROR: Kernel ELF not found at $KERNEL_ELF"
    echo "Run 'make build' first."
    exit 1
fi

# Check Limine is available
if [ -z "$LIMINE_DATADIR" ] || [ ! -d "$LIMINE_DATADIR" ]; then
    echo "ERROR: Limine not found. Install Limine first."
    echo "  brew install limine  (or see https://github.com/limine-bootloader/limine)"
    exit 1
fi

# Create ISO tree
ISO_TREE="$ROOT_DIR/out/iso-tree"
rm -rf "$ISO_TREE"
mkdir -p "$ISO_TREE/boot/limine"
mkdir -p "$ISO_TREE/EFI/BOOT"

# Copy kernel
cp "$KERNEL_ELF" "$ISO_TREE/boot/kernel.elf"

# Copy initramfs if present
if [ -f "$INITRAMFS_IMG" ]; then
    cp "$INITRAMFS_IMG" "$ISO_TREE/boot/initramfs.img"
    echo "Included initramfs: $INITRAMFS_IMG"
else
    echo "WARNING: initramfs not found at $INITRAMFS_IMG"
    echo "  Run 'make initramfs' to generate it."
fi

# Copy Limine config
cp "$CONFIG_DIR/limine.conf" "$ISO_TREE/boot/limine/limine.conf"

# Copy Limine bootloader files
cp "$LIMINE_DATADIR/BOOTX64.EFI"         "$ISO_TREE/EFI/BOOT/BOOTX64.EFI"
cp "$LIMINE_DATADIR/BOOTIA32.EFI"        "$ISO_TREE/EFI/BOOT/BOOTIA32.EFI" 2>/dev/null || true
cp "$LIMINE_DATADIR/limine-bios.sys"     "$ISO_TREE/boot/limine/limine-bios.sys" 2>/dev/null || true
cp "$LIMINE_DATADIR/limine-bios-cd.bin"  "$ISO_TREE/boot/limine/limine-bios-cd.bin" 2>/dev/null || true
cp "$LIMINE_DATADIR/limine-uefi-cd.bin"  "$ISO_TREE/boot/limine/limine-uefi-cd.bin" 2>/dev/null || true

echo "ISO tree assembled at $ISO_TREE"

# Create ISO
mkdir -p "$(dirname "$OUTPUT_ISO")"

xorriso -as mkisofs -R -r -J \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    "$ISO_TREE" \
    -o "$OUTPUT_ISO"

limine bios-install "$OUTPUT_ISO" 2>/dev/null || true

echo ""
echo "============================================="
echo "  ISO created: $OUTPUT_ISO"
echo "  Size: $(du -h "$OUTPUT_ISO" | cut -f1)"
echo "============================================="
