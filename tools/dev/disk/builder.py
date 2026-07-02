import platform
from config import DiskConfig
from backends import LinuxBackend, MacBackend


class DiskBuilder:

    def __init__(self):
        os_name = platform.system()
        if os_name == "Linux":
            self.backend = LinuxBackend()
        elif os_name == "Darwin":
            self.backend = MacBackend()
        else:
            raise Exception("Unsupported OS")

    def build(self, config: DiskConfig):
        self.backend.create_image(config)
        self.backend.create_gpt(config)
        self.backend.add_partitions(config)
        self.backend.format_and_copy(config)
        self.backend.detach()
