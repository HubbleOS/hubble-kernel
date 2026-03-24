from abc import ABC, abstractmethod
from config import DiskConfig


class Backend(ABC):

    @abstractmethod
    def create_image(self, config: DiskConfig): pass

    @abstractmethod
    def create_gpt(self, config: DiskConfig): pass

    @abstractmethod
    def add_partitions(self, config: DiskConfig): pass

    @abstractmethod
    def format_and_copy(self, config: DiskConfig): pass

    @abstractmethod
    def detach(self): pass
