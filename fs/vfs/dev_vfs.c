#include "vfs.h"
#include "vfs_standart_struct.h"
#include "dev.h"
#include <stdbool.h>
#include <hubble/string.h>
#include <hubble/printk.h>
#include "higher_half.h"

VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path);
VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path);
int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size);
uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size);
int dev_vfs_read_device(VFS_File *file, void *buf, uint32_t size);

static VFS_device_reg *dev_vfs_devices = NULL;

bool dev_vfs_init(VFS_FS *fs, VFS_Device *device, uint32_t start_lba)
{
	printk(KERN_INFO "Initializing device fs\n");

	fs->create_file = dev_vfs_create_device;
	fs->open = dev_vfs_open_device;
	fs->write = dev_vfs_write_device;
	fs->read = dev_vfs_read_device;
	fs->fs = fs;
	return 1;
}

VFS_device_reg *dev_vfs_find_device(VFS_FS *fs, const char *path)
{
	VFS_device_reg *dev = dev_vfs_devices;
	while (dev)
	{
		if (strcmp(dev->name, path) == 0)
			return dev;
		dev = dev->next;
	}
	return NULL;
}

int dev_vfs_read_device(VFS_File *file, void *buf, uint32_t size)
{
	VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
	if (dev->read)
		dev->read(0, size, buf);
	file->pos = 0;
	return size;
}

int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size)
{
	VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
	if (dev->write)
		dev->write(0, size, buf);
	return size;
}

VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path)
{
	VFS_device_reg *dev = dev_vfs_find_device(fs, path);
	if (dev)
	{
		VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
		memset(node, 0, sizeof(VFS_Node));
		strncpy(node->name, path, 255);
		node->fs = fs;
		node->fs_node = dev;
		node->size = 1;
		return node;
	}
	return NULL;
}

VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path)
{
	VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
	memset(node, 0, sizeof(VFS_Node));
	strncpy(node->name, path, 255);
	node->fs = fs;

	VFS_device_reg *dev = kmalloc(sizeof(VFS_device_reg), GFP_KERNEL);
	memset(dev, 0, sizeof(VFS_device_reg));
	strncpy(dev->name, path, 31);

	dev->mmap = NULL;
	dev->read = NULL;
	dev->write = NULL;
	dev->next = dev_vfs_devices;
	dev_vfs_devices = dev;

	node->fs_node = dev;
	return node;
}

void dev_vfs_register(const char *name,
		      uint64_t (*mmap)(uint64_t, size_t),
		      uint64_t (*read)(uint64_t, size_t, void *),
		      uint64_t (*write)(uint64_t, size_t, const void *))
{
	VFS_device_reg *dev = kmalloc(sizeof(VFS_device_reg), GFP_KERNEL);
	memset(dev, 0, sizeof(VFS_device_reg));
	strncpy(dev->name, name, 31);
	dev->mmap = mmap;
	dev->read = read;
	dev->write = write;
	dev->next = dev_vfs_devices;
	dev_vfs_devices = dev;
}

uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size)
{
	VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
	if (dev->mmap)
		return dev->mmap(offset, size);
	return 0;
}
