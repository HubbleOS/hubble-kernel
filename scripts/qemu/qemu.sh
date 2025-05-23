#!/bin/bash

# if [-z "$1"]; then
# 	echo "Usage: $0 <iso>"
# 	exit 1
# fi

# ISO_DIR=$1

WORKDIR="$(pwd)"
ISO_DIR="$WORKDIR/out/x86/iso"

if [ ! -d "$ISO_DIR" ]; then
	echo "❌ Directory not found: $ISO_DIR"
	exit 1
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
