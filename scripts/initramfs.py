#!/usr/bin/env python3

"""
initramfs.py - CPIO newc archive generator for Hubble Kernel.

Creates a deterministic CPIO "newc" archive from a directory tree.

The resulting archive is used as the initramfs loaded by Limine.

This tool is part of hubble-kernel and is designed to be used by
distributions (such as Hubble OS) to package their rootfs payloads.

Supported filesystem entries:
    - regular files
    - directories
    - symbolic links

Properties:
    - preserves file permissions
    - preserves symbolic links
    - deterministic entry ordering
    - deterministic timestamps
    - root ownership (uid=0, gid=0)
    - fail-fast on filesystem errors

Usage:

    python3 scripts/initramfs.py \
        --root rootfs \
        --output build/initramfs.img

CPIO newc format:

    110-byte ASCII hexadecimal header
    filename + NUL
    4-byte alignment
    file data
    4-byte alignment
"""

from __future__ import annotations

import argparse
import os
import stat
import sys
from dataclasses import dataclass
from pathlib import Path


CPIO_MAGIC = b"070701"
CPIO_TRAILER = "TRAILER!!!"
CPIO_HEADER_SIZE = 110

# CPIO "newc" archives use 8 ASCII hex digits for numeric fields.
CPIO_FIELD_WIDTH = 8

# Reproducible initramfs timestamp.
DEFAULT_MTIME = 0

# Hubble initramfs uses root ownership.
DEFAULT_UID = 0
DEFAULT_GID = 0


@dataclass(frozen=True)
class CpioEntry:
    """
    A single filesystem entry in the CPIO archive.
    """

    name: str
    mode: int
    data: bytes
    uid: int = DEFAULT_UID
    gid: int = DEFAULT_GID
    mtime: int = DEFAULT_MTIME
    inode: int = 0
    nlink: int = 1
    dev_major: int = 0
    dev_minor: int = 0
    rdev_major: int = 0
    rdev_minor: int = 0


def align4(value: int) -> int:
    """
    Round value up to the next 4-byte boundary.
    """
    return (value + 3) & ~3


def cpio_hex(value: int) -> bytes:
    """
    Encode an integer as an 8-character lowercase hexadecimal ASCII field.
    """
    if value < 0:
        raise ValueError(f"CPIO field cannot be negative: {value}")

    if value > 0xFFFFFFFF:
        raise ValueError(
            f"CPIO field exceeds 32-bit range: {value}"
        )

    return f"{value:08x}".encode("ascii")


def build_cpio_entry(entry: CpioEntry) -> bytes:
    """
    Serialize one CPIO newc entry.
    """

    name_bytes = entry.name.encode("utf-8") + b"\x00"
    namesize = len(name_bytes)
    filesize = len(entry.data)

    header = bytearray()

    header += CPIO_MAGIC

    header += cpio_hex(entry.inode)
    header += cpio_hex(entry.mode)
    header += cpio_hex(entry.uid)
    header += cpio_hex(entry.gid)
    header += cpio_hex(entry.nlink)
    header += cpio_hex(entry.mtime)
    header += cpio_hex(filesize)
    header += cpio_hex(entry.dev_major)
    header += cpio_hex(entry.dev_minor)
    header += cpio_hex(entry.rdev_major)
    header += cpio_hex(entry.rdev_minor)
    header += cpio_hex(namesize)

    # "check" field.
    # For CPIO newc (070701), this must be zero.
    header += cpio_hex(0)

    if len(header) != CPIO_HEADER_SIZE:
        raise AssertionError(
            f"Invalid CPIO header size: {len(header)}"
        )

    result = bytearray()

    result += header

    # Filename.
    result += name_bytes

    # Filename padding.
    result += b"\x00" * (align4(namesize) - namesize)

    # File data.
    result += entry.data

    # File data padding.
    result += b"\x00" * (align4(filesize) - filesize)

    return bytes(result)


def make_entry(
    relative_path: Path,
    file_stat: os.stat_result,
    data: bytes,
) -> CpioEntry:
    """
    Create a CPIO entry from a filesystem stat structure.
    """

    mode = file_stat.st_mode

    return CpioEntry(
        name=relative_path.as_posix(),
        mode=mode,
        data=data,
        uid=DEFAULT_UID,
        gid=DEFAULT_GID,
        mtime=DEFAULT_MTIME,
        inode=file_stat.st_ino & 0xFFFFFFFF,
        nlink=1,
    )


def collect_entries(root: Path) -> list[CpioEntry]:
    """
    Walk the root filesystem and convert it into CPIO entries.

    The resulting list is sorted by relative path to guarantee
    deterministic archive generation.
    """

    entries: list[CpioEntry] = []

    def walk(directory: Path) -> None:
        try:
            children = list(directory.iterdir())
        except OSError as exc:
            raise RuntimeError(
                f"failed to read directory '{directory}': {exc}"
            ) from exc

        # Sort by filesystem path for deterministic output.
        children.sort(key=lambda path: path.name)

        for path in children:
            relative_path = path.relative_to(root)

            try:
                file_stat = path.lstat()
            except OSError as exc:
                raise RuntimeError(
                    f"failed to stat '{path}': {exc}"
                ) from exc

            mode = file_stat.st_mode

            if stat.S_ISDIR(mode):
                entries.append(
                    make_entry(
                        relative_path=relative_path,
                        file_stat=file_stat,
                        data=b"",
                    )
                )

                # Do not follow symlinks because lstat() + S_ISDIR()
                # guarantees that only real directories are traversed.
                walk(path)

                continue

            if stat.S_ISREG(mode):
                try:
                    data = path.read_bytes()
                except OSError as exc:
                    raise RuntimeError(
                        f"failed to read '{path}': {exc}"
                    ) from exc

                entries.append(
                    make_entry(
                        relative_path=relative_path,
                        file_stat=file_stat,
                        data=data,
                    )
                )

                continue

            if stat.S_ISLNK(mode):
                try:
                    target = os.readlink(path)
                except OSError as exc:
                    raise RuntimeError(
                        f"failed to read symlink '{path}': {exc}"
                    ) from exc

                entries.append(
                    make_entry(
                        relative_path=relative_path,
                        file_stat=file_stat,
                        data=target.encode("utf-8"),
                    )
                )

                continue

            file_type = stat.filemode(mode)

            raise RuntimeError(
                f"unsupported filesystem entry '{path}' "
                f"({file_type})"
            )

    walk(root)

    # Final canonical ordering.
    entries.sort(key=lambda entry: entry.name)

    return entries


def build_initramfs(root_dir: Path) -> bytes:
    """
    Build a complete CPIO newc archive from root_dir.
    """

    root = root_dir.resolve()

    if not root.exists():
        raise RuntimeError(
            f"root directory does not exist: {root}"
        )

    if not root.is_dir():
        raise RuntimeError(
            f"root path is not a directory: {root}"
        )

    entries = collect_entries(root)

    archive = bytearray()

    for entry in entries:
        archive += build_cpio_entry(entry)

    # End-of-archive marker.
    trailer = CpioEntry(
        name=CPIO_TRAILER,
        mode=0,
        data=b"",
        uid=0,
        gid=0,
        mtime=0,
        inode=0,
        nlink=1,
    )

    archive += build_cpio_entry(trailer)

    return bytes(archive)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a CPIO newc initramfs image."
    )

    parser.add_argument(
        "--root",
        type=Path,
        required=True,
        help="Source root filesystem directory.",
    )

    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Output initramfs image.",
    )

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        archive = build_initramfs(args.root)

        args.output.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        args.output.write_bytes(archive)

    except (OSError, RuntimeError, ValueError) as exc:
        print(
            f"initramfs: error: {exc}",
            file=sys.stderr,
        )
        return 1

    print(
        f"initramfs: {len(archive)} bytes -> {args.output}"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
