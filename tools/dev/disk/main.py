import os
from config import DiskConfig, Partition
from builder import DiskBuilder

arch = os.environ.get("ARCH", "x86")

config = DiskConfig(
    path="out/disks/disk.img",
    size=210,
    partitions=[
        Partition(
            label="BOOT",
            fs="fat32",
            size="100",
            files=[
                ("out/usr", "/usr/bin"),
                ("out/modules", "/modules"),
                ("out/busy", "/busy")
            ]
        ),
        Partition(
            label="ROOT",
            fs="ext2",
            size="100",
            files=[
                ("out/usr", "/usr/bin"),
                ("out/modules", "/modules"),
                ("out/busy", "/busy")
            ]
        )
    ]
)

DiskBuilder().build(config)
