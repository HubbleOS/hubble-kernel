import plistlib
import subprocess as sp
import sys
from pathlib import Path

from backends.base import Backend

ISO_PATH = Path("out/hubble.iso").resolve()


class MacBackend(Backend):

    def _check_iso(self):
        if not ISO_PATH.is_file():
            print(f"ERROR: ISO not found: {ISO_PATH}")
            print("Run 'make iso' first to build the image.")
            sys.exit(1)

    def _diskutil_list(self):
        result = sp.run(
            ["diskutil", "list", "-plist"],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print("ERROR: diskutil list failed.")
            sys.exit(1)
        return plistlib.loads(result.stdout.encode())

    def _diskutil_info(self, device):
        result = sp.run(
            ["diskutil", "info", "-plist", device],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            return None
        return plistlib.loads(result.stdout.encode())

    def _find_removable_disks(self, plist):
        disks = []
        for entry in plist.get("AllDisksAndPartitions", []):
            identifier = entry.get("DeviceIdentifier", "")
            info = self._diskutil_info(identifier)
            if info is None:
                continue
            if not info.get("RemovableMedia", False):
                continue
            size_bytes = info.get("Size", 0)
            size_gb = size_bytes / (1024 ** 3) if size_bytes else 0
            device_name = info.get("DeviceNode", identifier)
            disks.append({
                "path": f"/dev/{identifier}",
                "identifier": identifier,
                "size_gb": size_gb,
                "name": device_name,
            })
        return disks

    def _has_mounted_partitions(self, disk_path):
        result = sp.run(
            ["diskutil", "info", "-plist", disk_path],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            return False, []
        info = plistlib.loads(result.stdout.encode())
        mounted = []
        for part in info.get("Partitions", []):
            if part.get("Mounted", False):
                mounted.append(part.get("DeviceIdentifier", "unknown"))
        return len(mounted) > 0, mounted

    def disk_list(self):
        self._check_iso()

        plist = self._diskutil_list()
        disks = self._find_removable_disks(plist)

        if not disks:
            print("ERROR: No removable USB storage devices detected.")
            print("Plug in a USB drive and try again.")
            sys.exit(1)

        return disks

    def unmount_disk(self, disk):
        has_mounted, parts = self._has_mounted_partitions(disk)
        if has_mounted:
            print(f"\nERROR: Device {disk} has mounted partitions:")
            for part in parts:
                print(f"  /dev/{part}")
            print("Unmount them first: diskutil unmountDisk <disk>")
            sys.exit(1)

        result = sp.run(
            ["diskutil", "unmountDisk", disk],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"ERROR: Failed to unmount {disk}.")
            print(f"  {result.stderr.strip()}")
            sys.exit(1)

    def create_image(self):
        pass

    def format_image(self):
        pass

    def copy_files(self):
        pass

    def write_image(self, disk):
        rdisk = disk.replace("/dev/disk", "/dev/rdisk")
        size_mb = ISO_PATH.stat().st_size / (1024 * 1024)

        print(f"\n  Device : {disk} (raw: {rdisk})")
        print(f"  ISO    : {ISO_PATH}")
        print(f"  Size   : {size_mb:.1f} MB")
        print()
        print(f"  WARNING: ALL DATA on {disk} will be DESTROYED.")
        print()

        confirm = input(f"  Type the device name to confirm ({disk}): ").strip()
        if confirm != disk:
            print("Aborted. Confirmation did not match device name.")
            sys.exit(1)

        print(f"\nFlashing {ISO_PATH} to {rdisk} ...")
        result = sp.run([
            "sudo", "dd",
            f"if={ISO_PATH}",
            f"of={rdisk}",
            "bs=1m",
        ])
        if result.returncode != 0:
            print(f"ERROR: dd failed with exit code {result.returncode}")
            sys.exit(1)

        sp.run(["sync"])

    def detach(self, disk):
        sp.run(["diskutil", "eject", disk], capture_output=True)
        print(f"\nFlash complete: {disk}")
