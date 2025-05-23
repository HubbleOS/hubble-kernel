#!/bin/bash
set -e

WORKDIR="$(pwd)"

if [ -z "$1" ]; then
	echo "❌ Missing ISO directory"
	exit 1
elif [ ! -d "$1" ]; then
	echo "❌ Directory not found: $1"
	exit 1
else
	ISO_DIR="$1"
fi

qemu-system-x86_64 \
	-M pc \
	-cpu Haswell \
	-smp 2 \
	-m 1024 \
	-drive if=pflash,format=raw,readonly=on,file=$WORKDIR/ovmf/OVMF_CODE.fd \
	-drive if=pflash,format=raw,file=$WORKDIR/ovmf/OVMF_VARS.fd \
	-hda fat:rw:$ISO_DIR \
	-serial stdio
