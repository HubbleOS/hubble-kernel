#!/usr/bin/env python3
"""
mkdisk.py - Build a GPT disk image for testing the kernel's storage code.

    mkdisk.py --output out/disk.img \\
        --part fat32:BOOT:64:usr/disk:out/modules=/modules \\
        --part ext2:ROOT:64:usr/disk:out/initramfs-build/rootfs/bin=/bin

Each --part is FS:LABEL:SIZE_MB followed by sources: a directory whose
contents go to the partition's root, or DIR=/PATH to put them under PATH.
FS is fat32 or ext2. Partitions are 1 MiB aligned, in the order given.

No root needed: each filesystem is made in a file of its own (mke2fs -d
for ext2, mkfs.fat and mtools for FAT32), the GPT is written here, and
the filesystems are copied into place. FAT32 cannot hold symlinks, so
they are left out there.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import uuid
import zlib

SECTOR = 512
MIB = 1024 * 1024
ALIGN = MIB // SECTOR  # 2048 sectors
ENTRIES = 128
ENTRY_SIZE = 128
ENTRY_SECTORS = ENTRIES * ENTRY_SIZE // SECTOR  # 32

TYPES = {
    "fat32": uuid.UUID("EBD0A0A2-B9E5-4433-87C0-68B6B72699C7"),  # basic data
    "ext2": uuid.UUID("0FC63DAF-8483-4772-8E79-3D69D8477DE4"),  # Linux fs
}


def die(msg):
    sys.exit(f"mkdisk: {msg}")


def need(tool):
    if not shutil.which(tool):
        die(f"{tool} not found (install e2fsprogs, dosfstools and mtools)")


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        die(f"{' '.join(cmd)} failed:\n{r.stdout}{r.stderr}")


def parse_part(spec):
    fields = spec.split(":")
    if len(fields) < 3 or fields[0] not in TYPES or not fields[2].isdigit():
        die(f"bad --part '{spec}' (want FS:LABEL:SIZE_MB[:SRC...], FS fat32 or ext2)")
    sources = []
    for src in fields[3:]:
        path, _, dest = src.partition("=")
        sources.append((path, dest or "/"))
    return {"fs": fields[0], "label": fields[1], "mb": int(fields[2]),
            "sources": sources}


def stage(sources, into):
    """Copy the sources into one tree, the way they appear on the disk."""
    for path, dest in sources:
        if not os.path.isdir(path):
            print(f"mkdisk: note: {path} does not exist, skipped")
            continue
        shutil.copytree(path, os.path.join(into, dest.lstrip("/")),
                        symlinks=True, dirs_exist_ok=True)


def make_ext2(img, part, tree):
    need("mke2fs")
    run(["mke2fs", "-q", "-F", "-t", "ext2", "-L", part["label"],
         "-E", "root_owner=0:0", "-d", tree, img, f"{part['mb']}M"])


def make_fat32(img, part, tree):
    need("mkfs.fat")
    need("mcopy")
    if part["mb"] < 33:
        die(f"FAT32 partition {part['label']} needs at least 33 MB")
    run(["mkfs.fat", "-F", "32", "-n", part["label"][:11].upper(), "-C", img,
         str(part["mb"] * 1024)])
    for root, dirs, files in os.walk(tree):
        rel = os.path.relpath(root, tree)
        target = "::" if rel == "." else "::/" + rel
        for d in dirs:
            if not os.path.islink(os.path.join(root, d)):
                run(["mmd", "-i", img, f"{target}/{d}"])
        for f in files:
            src = os.path.join(root, f)
            if not os.path.islink(src):
                run(["mcopy", "-i", img, "-o", src, f"{target}/{f}"])


def gpt(parts, total):
    """Protective MBR, primary and backup GPT for a disk of total sectors."""
    last = total - 1
    entries = bytearray(ENTRIES * ENTRY_SIZE)
    for i, p in enumerate(parts):
        name = p["label"].encode("utf-16-le")[:72]
        struct.pack_into("<16s16sQQQ72s", entries, i * ENTRY_SIZE,
                         TYPES[p["fs"]].bytes_le, uuid.uuid4().bytes_le,
                         p["first"], p["last"], 0, name)
    entries_crc = zlib.crc32(entries)
    disk_guid = uuid.uuid4().bytes_le

    def header(current, backup, entries_lba):
        h = bytearray(struct.pack(
            "<8sIIIIQQQQ16sQIII", b"EFI PART", 0x00010000, 92, 0, 0,
            current, backup, 2 + ENTRY_SECTORS, last - 1 - ENTRY_SECTORS,
            disk_guid, entries_lba, ENTRIES, ENTRY_SIZE, entries_crc))
        struct.pack_into("<I", h, 16, zlib.crc32(h))
        return bytes(h).ljust(SECTOR, b"\0")

    mbr = bytearray(SECTOR)
    struct.pack_into("<B3sB3sII", mbr, 446, 0, b"\x00\x02\x00", 0xEE,
                     b"\xff\xff\xff", 1, min(total - 1, 0xFFFFFFFF))
    mbr[510:512] = b"\x55\xaa"
    return [
        (0, bytes(mbr)),
        (1, header(1, last, 2)),
        (2, bytes(entries)),
        (last - ENTRY_SECTORS, bytes(entries)),
        (last, header(last, 1, last - ENTRY_SECTORS)),
    ]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--output", required=True)
    ap.add_argument("--part", action="append", required=True, type=parse_part)
    args = ap.parse_args()

    lba = ALIGN
    for p in args.part:
        p["first"] = lba
        p["last"] = lba + p["mb"] * ALIGN - 1
        lba += p["mb"] * ALIGN
    total = lba + ALIGN  # room for the backup GPT, 1 MiB aligned

    out = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    tmp_out = out + ".tmp"
    with tempfile.TemporaryDirectory(dir=os.path.dirname(out)) as work:
        with open(tmp_out, "wb") as disk:
            disk.truncate(total * SECTOR)
            for i, p in enumerate(args.part):
                tree = os.path.join(work, f"tree{i}")
                os.makedirs(tree)
                stage(p["sources"], tree)
                img = os.path.join(work, f"part{i}.img")
                (make_ext2 if p["fs"] == "ext2" else make_fat32)(img, p, tree)
                disk.seek(p["first"] * SECTOR)
                with open(img, "rb") as f:
                    shutil.copyfileobj(f, disk)
            for sector, data in gpt(args.part, total):
                disk.seek(sector * SECTOR)
                disk.write(data)
    os.replace(tmp_out, out)

    for i, p in enumerate(args.part, 1):
        print(f"mkdisk: partition {i}: {p['fs']} {p['label']} {p['mb']} MB "
              f"(LBA {p['first']}..{p['last']})")
    print(f"mkdisk: {total * SECTOR // MIB} MB -> {out}")


if __name__ == "__main__":
    main()
