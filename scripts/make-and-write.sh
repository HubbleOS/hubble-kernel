#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "$SCRIPT_DIR"

sh "$SCRIPT_DIR/make-image.sh"
sh "$SCRIPT_DIR/write-to-usb.sh"
