#!/bin/bash
#
# run-qemu.sh — Run the kernel in QEMU for development.
#
# Usage:
#   ./scripts/qemu/run-qemu.sh [OPTIONS]
#
# Options:
#   --debug     Enable GDB debug mode (adds -s -S flags)
#   --iso PATH  Use specific ISO (default: out/kernel-dev.iso)
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

DEBUG=false
ISO_PATH="$ROOT_DIR/out/kernel-dev.iso"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            DEBUG=true
            shift
            ;;
        --iso)
            ISO_PATH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check ISO exists
if [ ! -f "$ISO_PATH" ]; then
    echo "ERROR: ISO not found at $ISO_PATH"
    echo "Run 'make run' or 'make debug' first."
    exit 1
fi

# Check QEMU is available
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "ERROR: qemu-system-x86_64 not found in PATH"
    echo "Install QEMU first."
    exit 1
fi

# Build QEMU command
QEMU_CMD=(
    qemu-system-x86_64
    -M pc
    -cpu Haswell
    -m 256M
    -smp 10
    # -accel tcg,thread=on
    -cdrom "$ISO_PATH"
    -serial stdio
    -debugcon file:/tmp/debugcon.log
    -no-reboot
    -no-shutdown
)

if [ "$DEBUG" = true ]; then
    QEMU_CMD+=(-s -S)
    echo "=== Hubble Kernel QEMU (DEBUG MODE) ==="
    echo "GDB stub listening on port 1234"
    echo "CPU paused - connect GDB and run 'continue'"
else
    QEMU_CMD+=(-d int -D /tmp/qemu-kernel.log)
    echo "=== Hubble Kernel QEMU ==="
fi

echo "QEMU command: ${QEMU_CMD[*]}"
echo ""

exec "${QEMU_CMD[@]}"
