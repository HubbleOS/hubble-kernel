import platform

from backends import MacBackend


class Builder:
    def __init__(self):
        os_name = platform.system()

        if os_name == "Linux":
            print("Linux backend not implemented")
        elif os_name == "Darwin":
            self.backend = MacBackend()
        else:
            raise Exception("Unsupported OS")

    def build(self):
        disks = self.backend.disk_list()

        if not disks:
            raise Exception("No removable disks found")

        print("\nAvailable disks:")
        for i, disk in enumerate(disks):
            print(f"{i + 1}. {disk}")

        choice = int(input("\nSelect disk: ")) - 1
        disk = disks[choice]

        print(f"\nSelected: {disk}")

        self.backend.unmount_disk(disk)

        self.backend.create_image()
        self.backend.format_image()
        self.backend.copy_files()

        self.backend.write_image(disk)
        self.backend.detach(disk)
