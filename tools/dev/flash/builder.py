import platform

from backends import LinuxBackend, MacBackend


class Builder:
    def __init__(self):
        os_name = platform.system()

        if os_name == "Linux":
            self.backend = LinuxBackend()
        elif os_name == "Darwin":
            self.backend = MacBackend()
        else:
            raise Exception("make flash is only supported on Linux and macOS")

    def build(self):
        disks = self.backend.disk_list()

        if not disks:
            raise Exception("No removable disks found")

        print("\nAvailable disks:")
        for i, disk in enumerate(disks):
            if isinstance(disk, dict):
                print(f"  {i + 1}. {disk['path']}  ({disk.get('size_gb', 0):.1f} GB)  {disk.get('name', '')}")
            else:
                print(f"  {i + 1}. {disk}")

        choice = int(input("\nSelect disk: ")) - 1
        if isinstance(disks[0], dict):
            disk = disks[choice]["path"]
        else:
            disk = disks[choice]

        print(f"\nSelected: {disk}")

        self.backend.unmount_disk(disk)

        self.backend.create_image()
        self.backend.format_image()
        self.backend.copy_files()

        self.backend.write_image(disk)
        self.backend.detach(disk)
