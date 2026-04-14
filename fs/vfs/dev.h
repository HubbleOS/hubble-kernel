#pragma once

#include "vfs.h"
#include "vfs_standart_struct.h"
#include <stdbool.h>

typedef struct
{
	uint64_t (*mmap)(uint64_t offset, size_t size);
	uint64_t (*read)(uint64_t offset, size_t size);
} VFS_device_file;

typedef struct VFS_device_reg
{
	char name[32];
	uint64_t (*mmap)(uint64_t offset, size_t size);
	uint64_t (*read)(uint64_t offset, size_t size, void *buf);
	uint64_t (*write)(uint64_t offset, size_t size, const void *buf);
	struct VFS_device_reg *next;
} VFS_device_reg;

VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path);
VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path);
int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size);
uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size);
VFS_device_reg *dev_vfs_find_device(VFS_FS *fs, const char *path);
void dev_vfs_register(const char *name,
		      uint64_t (*mmap)(uint64_t, size_t),
		      uint64_t (*read)(uint64_t, size_t, void *),
		      uint64_t (*write)(uint64_t, size_t, const void *));
