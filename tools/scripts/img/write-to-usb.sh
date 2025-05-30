#!/bin/bash
set -e

WORKDIR="$(pwd)"
ARCH="x86"
OUTPUT_DIR="$WORKDIR/out"

IMAGE="$OUTPUT_DIR/$ARCH/fat.img"

if [ ! -f "$IMAGE" ]; then
	echo "❌ Image $IMAGE not found. Run make-image.sh first."
	exit 1
fi

echo "🔎 Platform definition..."
OS="$(uname -s)"

case "$OS" in
Linux)
	echo "🔧 Linux: looking for available devices..."
	lsblk -d -o NAME,SIZE,MODEL
	echo
	read -rp "Input name of device (for example, /dev/sdX): " DEVICE
	sudo dd if="$IMAGE" of="$DEVICE" bs=1M status=progress
	sync
	echo "✅ Image written to $DEVICE"
	;;

Darwin)
	echo "🔧 macOS: looking for available devices..."
	diskutil list
	echo
	read -rp "Input name of device (for example, disk2): " DISK
	diskutil unmountDisk /dev/"$DISK"
	sudo dd if="$IMAGE" of=/dev/r"$DISK" bs=1m status=progress
	sync
	diskutil eject /dev/"$DISK"
	echo "✅ Image written to /dev/$DISK"
	;;

*)
	echo "❌ OS NOT SUPPORTED"
	exit 1
	;;
esac
