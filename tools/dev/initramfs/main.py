#!/usr/bin/env python3
"""
initramfs.py - CPIO newc archive generator for Hubble OS.

Creates a CPIO newc archive from a directory tree.
This archive becomes the initramfs loaded by Limine as a module.

Usage:
    python3 initramfs.py --root <source_dir> --output <initramfs.img>

CPIO newc format: each entry has a 110-byte ASCII header followed by
4-byte-aligned filename and file data.
"""

import argparse
import os
import struct
import sys
from pathlib import Path


CPIO_MAGIC = b"070701"
CPIO_TRAILER = b"TRAILER!!!\x00"
CPIO_HEADER_SIZE = 110


def cpio_hex(value, width=8):
    """Format an integer as zero-padded hex ASCII."""
    return f"{value:0{width}x}".encode("ascii")


def cpio_entry(name, mode, data=b""):
    """Build a single CPIO newc entry."""
    name_bytes = name.encode("utf-8") + b"\x00"
    namesize = len(name_bytes)
    filesize = len(data)

    header = bytearray(CPIO_HEADER_SIZE)
    offset = 0
    header[offset:offset + 6] = CPIO_MAGIC
    offset += 6     # ino
    offset += 8     # mode
    header[offset:offset + 8] = cpio_hex(mode)
    offset += 8     # uid
    offset += 8     # gid
    offset += 8     # nlink
    offset += 8     # mtime
    offset += 8     # filesize
    header[offset:offset + 8] = cpio_hex(filesize)
    offset += 8     # devmajor
    offset += 8     # devminor
    offset += 8     # rdevmajor
    offset += 8     # rdevminor
    offset += 8     # namesize
    header[offset:offset + 8] = cpio_hex(namesize)
    offset += 8     # check (always 0 for 070701)
    # Total header is 110 bytes, rest is zeros (already zeroed)

    # Align helper
    def align4(v):
        return (v + 3) & ~3

    entry = bytes(header) + name_bytes
    # Pad name to 4-byte alignment
    name_padding = align4(namesize) - namesize
    entry += b"\x00" * name_padding

    entry += data
    # Pad data to 4-byte alignment
    data_padding = align4(filesize) - filesize
    entry += b"\x00" * data_padding

    return entry


def build_initramfs(root_dir):
    """Walk root_dir and produce a CPIO newc archive as bytes."""
    root = Path(root_dir).resolve()
    if not root.is_dir():
        print(f"Error: {root} is not a directory", file=sys.stderr)
        sys.exit(1)

    archive = bytearray()

    # Collect all entries (directories first via sorted walk)
    entries = []

    # Walk the directory tree
    for dirpath, dirnames, filenames in os.walk(root):
        rel = os.path.relpath(dirpath, root)
        if rel == ".":
            rel = ""

        # Add directory entry
        entries.append((rel, True, 0o040755, b""))

        # Add files
        for fname in sorted(filenames):
            fpath = os.path.join(dirpath, fname)
            file_rel = os.path.join(rel, fname) if rel else fname
            try:
                with open(fpath, "rb") as f:
                    data = f.read()
                entries.append((file_rel, False, 0o100644, data))
            except Exception as e:
                print(f"Warning: skipping {fpath}: {e}", file=sys.stderr)

    # Write entries
    for name, is_dir, mode, data in entries:
        if is_dir:
            archive += cpio_entry(name + "/", mode, b"")
        else:
            archive += cpio_entry(name, mode, data)

    # End-of-archive trailer
    archive += cpio_entry("TRAILER!!!", 0, b"")

    return bytes(archive)


def main():
    parser = argparse.ArgumentParser(
        description="Generate CPIO newc initramfs image"
    )
    parser.add_argument(
        "--root", required=True, help="Source directory to pack"
    )
    parser.add_argument(
        "--output", required=True, help="Output initramfs image path"
    )
    args = parser.parse_args()

    archive = build_initramfs(args.root)

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "wb") as f:
        f.write(archive)

    print(f"initramfs: {len(archive)} bytes -> {output}")


if __name__ == "__main__":
    main()
