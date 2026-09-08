import subprocess as sp
import sys
from pathlib import Path

from backends.base import Backend

ISO_PATH = Path("out/hubble.iso").resolve()


class LinuxBackend(Backend):

    def _check_iso(self):
        if not ISO_PATH.is_file():
            print(f"ERROR: ISO not found: {ISO_PATH}")
            print("Run 'make iso' first to build the image.")
            sys.exit(1)

    def _lsblk_json(self):
        result = sp.run(
            ["lsblk", "-J", "-o",
             "NAME,SIZE,TYPE,RM,RO,TRAN,MOUNTPOINT,FSTYPE"],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print("ERROR: lsblk failed. Is util-linux installed?")
            sys.exit(1)
        import json
        return json.loads(result.stdout)

    def _find_removable_devices(self, blkdata):
        devices = []
        for disk in blkdata.get("blockdevices", []):
            if disk.get("type") != "disk":
                continue
            if disk.get("rm") is not True:
                continue
            devices.append(disk)
        return devices

    def _get_mounted_partitions(self, devpath):
        result = sp.run(
            ["findmnt", "-r", "-n", "-o", "SOURCE"],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            return []
        mounted = []
        for line in result.stdout.strip().splitlines():
            if line.startswith(devpath):
                mounted.append(line)
        return mounted

    def disk_list(self):
        self._check_iso()

        blkdata = self._lsblk_json()
        devices = self._find_removable_devices(blkdata)

        if not devices:
            print("ERROR: No removable USB storage devices detected.")
            print("Plug in a USB drive and try again.")
            sys.exit(1)

        return [f"/dev/{dev['name']}" for dev in devices]

    def unmount_disk(self, disk):
        mounted = self._get_mounted_partitions(disk)
        if mounted:
            print(f"\nERROR: Device {disk} has mounted partitions:")
            for part in mounted:
                print(f"  {part}")
            print("Unmount them first: sudo umount <partition>")
            sys.exit(1)

    def create_image(self):
        pass

    def format_image(self):
        pass

    def copy_files(self):
        pass

    def write_image(self, disk):
        size_mb = ISO_PATH.stat().st_size / (1024 * 1024)

        print(f"\n  Device : {disk}")
        print(f"  ISO    : {ISO_PATH}")
        print(f"  Size   : {size_mb:.1f} MB")
        print()
        print(f"  WARNING: ALL DATA on {disk} will be DESTROYED.")
        print()

        confirm = input(f"  Type the device name to confirm ({disk}): ").strip()
        if confirm != disk:
            print("Aborted. Confirmation did not match device name.")
            sys.exit(1)

        print(f"\nFlashing {ISO_PATH} to {disk} ...")
        result = sp.run(
            [
                "sudo", "dd",
                f"if={ISO_PATH}",
                f"of={disk}",
                "bs=4M",
                "oflag=sync",
                "status=progress",
            ]
        )
        if result.returncode != 0:
            print(f"ERROR: dd failed with exit code {result.returncode}")
            sys.exit(1)

        sp.run(["sync"])

    def detach(self, disk):
        print(f"\nFlash complete: {disk}")
