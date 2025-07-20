#!/bin/bash
set -e

WORKDIR="$(pwd)"

# Read debug port from config file or use default
CONFIG_FILE="$WORKDIR/.config"
DEFAULT_PORT=1234

if [ -f "$CONFIG_FILE" ]; then
	PORT=$(grep -E '^DEBUG_PORT=' "$CONFIG_FILE" | cut -d '=' -f 2)
fi

if [ -z "$PORT" ]; then
	PORT=$DEFAULT_PORT
fi

echo "Using debug port: $PORT"

# Check if ISO directory is provided and exists
if [ -z "$1" ]; then
	echo "❌ Missing ISO directory"
	exit 1
elif [ ! -d "$1" ]; then
	echo "❌ Directory not found: $1"
	exit 1
else
	ISO_DIR="$1"
fi

# OS specific handling for ISO path
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
	WIN_ISO_DIR=$(cygpath -w "$ISO_DIR")
	QEMU_ISO_PATH="$WIN_ISO_DIR"
	;;
Linux | Darwin)
	QEMU_ISO_PATH="$ISO_DIR"
	;;
*)
	echo "❌ Unsupported OS: $(uname -s)"
	exit 1
	;;
esac

# Run QEMU with OVMF
qemu-system-x86_64 \
	-M pc \
	-cpu Haswell \
	-smp 2 \
	-m 2048 \
	-drive file=$WORKDIR/out/x86/disk.img,format=raw,index=0,media=disk,cache=none \
	-drive file=fat:rw:$QEMU_ISO_PATH,format=raw,index=1,media=disk \
	-drive if=pflash,format=raw,readonly=on,file=$WORKDIR/tools/scripts/qemu/ovmf/OVMF_CODE.fd \
	-serial stdio

# -S -gdb tcp::${PORT}
