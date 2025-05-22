#!/bin/bash

# if [-z "$1"]; then
# 	echo "Usage: $0 <iso>"
# 	exit 1
# fi

# ISO_DIR=$1

qemu-system-x86_64 \
	-M pc \
	-cpu Haswell \
	-smp 2 \
	-m 1024 \
	-drive if=pflash,format=raw,readonly=on,file=ovmf/OVMF_CODE.fd \
	-drive if=pflash,format=raw,file=ovmf/OVMF_VARS.fd \
	-hda fat:rw:iso/x86 \
	-serial stdio
