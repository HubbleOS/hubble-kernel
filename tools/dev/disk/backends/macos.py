import os
import plistlib
import subprocess
from config import DiskConfig
from backends.base import Backend


def run(cmd):
    print(">>", " ".join(cmd))
    subprocess.run(cmd, check=True)


class MacBackend(Backend):

    def create_image(self, config: DiskConfig):
        dmg_path = config.path + ".dmg"

        for p in (config.path, dmg_path):
            if os.path.exists(p):
                os.remove(p)
                print(f"Removed old image: {p}")

        run(["hdiutil", "create", "-size", f"{config.size_mb}m",
             "-layout", "GPTSPUD", "-o", config.path])

        if os.path.exists(dmg_path):
            os.rename(dmg_path, config.path)

        result = subprocess.run(
            ["hdiutil", "attach", "-nomount", config.path],
            check=True, capture_output=True, text=True
        )
        self._disk = result.stdout.strip().splitlines()[0].split()[0]
        print(f"Image attached as {self._disk}")

    def create_gpt(self, config: DiskConfig):
        print("GPT already created via hdiutil -layout GPTSPUD")

    def add_partitions(self, config: DiskConfig):
        fs_map = {"fat32": "FAT32", "exfat": "ExFAT", "ext4": "JHFS+"}
        args = ["diskutil", "partitionDisk", self._disk,
                str(len(config.partitions)), "GPT"]
        for p in config.partitions:
            args += [fs_map.get(p.fs, "ExFAT"), p.label,
                     p.size if p.size != "0" else "0"]
        run(args)

    def format_and_copy(self, config: DiskConfig):
        for i, p in enumerate(config.partitions):
            if not p.files:
                continue

            result = subprocess.run(
                ["diskutil", "info", "-plist", f"{self._disk}s{i+1}"],
                check=True, capture_output=True, text=True
            )
            info = plistlib.loads(result.stdout.encode())
            mnt = info.get("MountPoint", "")

            if not mnt:
                print(f"Partition {p.label} is not mounted, skipping")
                continue

            print(f"Partition {p.label} mounted at {mnt}")

            for src, dst in p.files:
                full_dst = os.path.join(mnt, dst.lstrip("/"))
                os.makedirs(os.path.dirname(full_dst), exist_ok=True)
                run(["cp", "-r", src, full_dst])

    def detach(self):
        if hasattr(self, "_disk"):
            run(["hdiutil", "detach", self._disk])
