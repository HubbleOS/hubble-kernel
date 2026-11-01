This file comes from usr/disk/ in the kernel tree. Everything in that
folder is put on both partitions of out/disk.img (FAT32 BOOT and ext2
ROOT, mounted at /ext2) by `make disk`. Add files there to test the
kernel's filesystem code with them.
