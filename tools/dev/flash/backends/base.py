from abc import ABC, abstractmethod


class Backend(ABC):

    @abstractmethod
    def disk_list(self): pass

    @abstractmethod
    def unmount_disk(self): pass

    @abstractmethod
    def create_image(self): pass

    @abstractmethod
    def format_image(self): pass

    @abstractmethod
    def copy_files(self): pass

    @abstractmethod
    def write_image(self, disk): pass

    @abstractmethod
    def detach(self): pass
