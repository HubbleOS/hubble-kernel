#!/bin/bash
set -e

echo "Kernel Build System"
echo ""
echo "Usage:"
echo "  make [TARGET] [ARCH=<arch>]"
echo ""
echo "Targets:"
echo "  all      - Build the kernel (default)"
echo "  build    - Same as 'all'"
echo "  run      - Run the built kernel"
echo "  img      - Build the kernel image"
echo "  flash    - Flash the built kernel to a USB device"
echo "  clean    - Remove all build output"
echo ""
echo "Available ARCH values: x86, arm64 (extendable)"
