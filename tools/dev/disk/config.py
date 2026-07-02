from dataclasses import dataclass, field


@dataclass
class Partition:
    label: str
    fs: str          # "fat32" / "exfat" / "ext4"
    size: str        # "100M" / "0" (0 = remaining space)
    files: list[tuple[str, str]] = field(default_factory=list)


@dataclass
class DiskConfig:
    path: str
    size_mb: int
    partitions: list[Partition]
