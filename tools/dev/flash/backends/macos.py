import subprocess as sp
from pathlib import Path


from backends.base import Backend


class MacBackend(Backend):

    def disk_list(self):
        result = sp.run(
            ["diskutil", "list", "-plist"],
            capture_output=True,
            text=True
        )

        import plistlib

        plist = plistlib.loads(result.stdout.encode())

        disks = []

        for disk in plist["AllDisksAndPartitions"]:
            device = disk["DeviceIdentifier"]

            info = sp.run(
                ["diskutil", "info", "-plist", device],
                capture_output=True,
                text=True
            )

            disk_info = plistlib.loads(info.stdout.encode())

            if disk_info.get("RemovableMedia"):
                disks.append(f"/dev/{device}")

        return disks

    def unmount_disk(self, disk):
        return sp.run(["diskutil", "unmountDisk", disk])

    def create_image(self):
        return sp.run([
            "dd",
            "if=/dev/zero",
            "of=hubble.img",
            "bs=1M",
            "count=64",
        ], check=True)

    def format_image(self):
        return sp.run([
            "mkfs.vfat",
            "-F", "32",
            "-n", "UEFI",
            "hubble.img",
        ], check=True)

    def copy_files(self):
        files = [
            str(f)
            for f in Path("./out/build/x86/iso").iterdir()
        ]

        return sp.run(
            ["mcopy", "-i", "hubble.img", "-s", *files, "::"],
            check=True,
        )

    def write_image(self, disk):
        return sp.run([
            "sudo",
            "dd",
            "if=./hubble.img",
            f"of={disk}",
            "bs=1m",
            "status=progress"
        ])

    def detach(self, disk):
        return sp.run(["diskutil", "eject", disk])
