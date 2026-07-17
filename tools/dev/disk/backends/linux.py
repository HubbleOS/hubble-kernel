import os
import subprocess
from config import DiskConfig
from backends.base import Backend


def run(cmd):
    print(">>", " ".join(cmd))
    return subprocess.run(cmd)

class LinuxBackend(Backend):

    def create_image(self, config: DiskConfig):
        run(["dd", "if=/dev/zero",
             f"of={config.path}", "bs=1M", f"count={config.size_mb}"])

    def create_gpt(self, config: DiskConfig):
        run(["parted", config.path, "--script", "--", "mklabel", "gpt"])

    def add_partitions(self, config: DiskConfig):
        args = ["parted", config.path, "--script", "--"]
        offset = 1
        for p in config.partitions:
            end = str(offset + int(p.size[:-1])) + \
                "MiB" if p.size != "0" else "100%"
            args += ["mkpart", p.label, p.fs, f"{offset}MiB", end]
            if p.size != "0":
                offset += int(p.size[:-1])
        run(args)

    def format_and_copy(self, config: DiskConfig):
        result = subprocess.run(
            ["sudo", "losetup", "--find", "--show", "--partscan", config.path],
            check=True, capture_output=True, text=True
        )
        self._loop = result.stdout.strip()
        print(f"Loop device: {self._loop}")

        for i, p in enumerate(config.partitions, start=1):
            dev = f"{self._loop}p{i}"
            if p.fs == "fat32":
                run(["sudo","mkfs.vfat", "-F", "32", "-n", p.label.upper(), dev])
            elif p.fs == "ext2":
                run(["sudo","mkfs.ext2", "-L", p.label, dev])
            elif p.fs == "exfat":
                run(["sudo","mkfs.exfat", "-n", p.label, dev])

            if p.files:
                mnt = f"/tmp/mnt_{p.label}"
                #os.makedirs(mnt, exist_ok=True)
                run(["sudo","mkdir",mnt])
                run(["sudo","mount", dev, mnt])
                for src, dst in p.files:
                    full_dst = os.path.join(mnt, dst.lstrip("/"))
                    run(["sudo","mkdir",os.path.dirname(full_dst)])
                    #os.makedirs(os.path.dirname(full_dst), exist_ok=True)
                    run(["sudo","cp", "-r", f"{src}/.", full_dst])
                run(["sudo","umount", mnt])

    def detach(self):
        if hasattr(self, "_loop"):
            run(["sudo","losetup", "-d", self._loop])
