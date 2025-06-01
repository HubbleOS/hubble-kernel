#!/bin/bash
set -e

WORKDIR="$(pwd)"

CONFIG_FILE="$WORKDIR/.config"
DEFAULT_PORT=1234

GDBINIT_FILE="$WORKDIR/.gdbinit"

if [ -f "$GDBINIT_FILE" ]; then
	GDB_OPTS="--command=$GDBINIT_FILE"
else
	GDB_OPTS=""
fi

# Get debug port from .config or use default
if [ -f "$CONFIG_FILE" ]; then
	PORT=$(grep -E '^DEBUG_PORT=' "$CONFIG_FILE" | cut -d '=' -f 2)
fi

# Fallback to default if not set
if [ -z "$PORT" ]; then
	PORT=$DEFAULT_PORT
fi

echo "🚀 Using debug port: $PORT"

# Path to the ELF file
ELF_FILE="out/x86/build/kernel.elf"

# Check if ELF file exists
if [ ! -f "$ELF_FILE" ]; then
	echo "❌ ELF file not found: $ELF_FILE"
	exit 1
fi

# Start GDB and connect to QEMU's debug server
gdb $GDB_OPTS "$ELF_FILE" \
	-ex "file $ELF_FILE" \
	-ex "target remote localhost:${PORT}" \
	-ex "layout split" \
	-ex "break kernel_main" \
	-ex "continue"
