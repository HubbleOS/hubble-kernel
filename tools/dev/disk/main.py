from config import DiskConfig, Partition
from builder import DiskBuilder


config = DiskConfig(
    path="out/disks/disk.img",
    size_mb=64,
    partitions=[
        Partition(
            label="BOOT",
            fs="fat32",
            size="0",
            files=[
                ("out/usr", "/usr/bin"),
            ]
        ),
    ]
)

DiskBuilder().build(config)
