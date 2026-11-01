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
# Environment (set by `make run` from config.mk, see config.mk.example):
#   QEMU_ACCEL  kvm, hvf, tcg or auto (default: kvm if /dev/kvm is usable,
#               hvf on macOS, else tcg)
#   QEMU_MEM    memory (default 800M)
#   QEMU_SMP    CPUs (default 2)
#   QEMU_ARGS   extra arguments, split on spaces
#   QEMU_DISK   disk image for the IDE primary master, if it exists
#               (default: out/disk.img, from `make disk`)
#   QEMU_DISPLAY  -display backend, e.g. gtk, sdl, none (default: QEMU's)
#   QEMU_SERIAL   -serial target (default stdio; none, file:PATH, ...)
#   QEMU_LOG      -d items, e.g. int,cpu_reset (default int; none or empty: off)
#   QEMU_LOG_FILE where -d writes (default /tmp/qemu-kernel.log)
#   QEMU_NO_REBOOT, QEMU_NO_SHUTDOWN
#               y (default): stop on a triple fault / keep the window
#               after a shutdown, so the last screen stays readable
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

ACCEL="${QEMU_ACCEL:-auto}"
if [ "$ACCEL" = auto ]; then
    if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
        ACCEL=kvm
    elif [ "$(uname -s)" = Darwin ]; then
        ACCEL=hvf
    else
        ACCEL=tcg
    fi
fi
case "$ACCEL" in
    kvm|hvf) ACCEL_ARGS=(-accel "$ACCEL" -cpu host) ;;
    tcg)     ACCEL_ARGS=(-accel tcg,thread=multi -cpu max) ;;
    *)
        echo "ERROR: QEMU_ACCEL must be kvm, hvf, tcg or auto, not '$ACCEL'"
        exit 1
        ;;
esac

read -ra EXTRA_ARGS <<< "${QEMU_ARGS:-}"

DISK_ARGS=()
DISK="${QEMU_DISK-$ROOT_DIR/out/disk.img}"
if [ -n "$DISK" ] && [ -f "$DISK" ]; then
    DISK_ARGS=(-drive "file=$DISK,format=raw,if=ide,index=0")
fi

OPT_ARGS=()
[ -n "${QEMU_DISPLAY:-}" ] && OPT_ARGS+=(-display "$QEMU_DISPLAY")
[ "${QEMU_NO_REBOOT:-y}" = y ] && OPT_ARGS+=(-no-reboot)
[ "${QEMU_NO_SHUTDOWN:-y}" = y ] && OPT_ARGS+=(-no-shutdown)
# The -d log slows QEMU down a lot; not while debugging with GDB.
LOG="${QEMU_LOG-int}"
if [ -n "$LOG" ] && [ "$LOG" != none ] && [ "$DEBUG" = false ]; then
    OPT_ARGS+=(-d "$LOG" -D "${QEMU_LOG_FILE:-/tmp/qemu-kernel.log}")
fi

# Build QEMU command
QEMU_CMD=(
    qemu-system-x86_64
    "${ACCEL_ARGS[@]}"
    -M pc
    -m "${QEMU_MEM:-800M}"
    -smp "${QEMU_SMP:-2}"
    -cdrom "$ISO_PATH"
    -boot d
    ${DISK_ARGS[@]+"${DISK_ARGS[@]}"}
    -serial "${QEMU_SERIAL:-stdio}"
    -debugcon file:/tmp/debugcon.log
    ${OPT_ARGS[@]+"${OPT_ARGS[@]}"}
    -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0
    ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
)

if [ "$DEBUG" = true ]; then
    QEMU_CMD+=(-s -S)
    echo "=== Hubble Kernel QEMU (DEBUG MODE) ==="
    echo "GDB stub listening on port 1234"
    echo "CPU paused - connect GDB and run 'continue'"
else
    echo "=== Hubble Kernel QEMU ==="
fi

echo "QEMU command: ${QEMU_CMD[*]}"
echo ""

exec "${QEMU_CMD[@]}"
