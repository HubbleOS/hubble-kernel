#pragma once

#include <_cheader.h>

#include <stdint.h>
#include <stdbool.h>

#include "vfs_standart_struct.h"
#include <fs/gpt/gpt.h>

_Begin_C_Header

    // Типи ФС

    // Типи відкритих файлових дескрипторів
    typedef struct VFS_Node
{

	char name[256];

	bool is_dir;
	uint32_t size;
	uint32_t mode;
	uint32_t pos;

	void *fs_node;	   // внутрішній вказівник драйвера (наприклад FAT32_DirectoryEntry*)
	struct VFS_FS *fs; // яка ФС обслуговує
} VFS_Node;

typedef struct
{
	uint32_t flags;
	uint32_t pos;
	VFS_Node *node;
} VFS_File;

// Таблиця функцій для ФС
typedef struct VFS_FS
{
	FileSystemType type;
	void *fs;

	bool (*mount)(struct VFS_FS *fs, VFS_Device *device, uint32_t start_lba);
	void (*unmount)(struct VFS_FS *fs);
	VFS_Node *(*create_file)(struct VFS_FS *fs, const char *path);

	VFS_Node *(*open)(struct VFS_FS *fs, const char *path);
	int (*read)(VFS_File *file, void *buf, uint32_t size);
	int (*write)(VFS_File *file, const void *buf, uint32_t size);

	bool (*mkdir)(struct VFS_FS *fs, const char *path);
	bool (*unlink)(struct VFS_FS *fs, const char *path);

	int (*close)(VFS_File *file);
	Directory (*readdir)(struct VFS_FS *fs, const char *path);
} VFS_FS;
// typedef enum
// {
// 	DEV_ATA,
// 	DEV_USB,
// 	DEV_NVME,
// } DeviceType;

// Функції VFS
bool vfs_mount(gpt_partition_t *partition, FileSystemType type);
VFS_File *vfs_open(const char *path, int flags);
int vfs_read(VFS_File *node, void *buf, uint32_t size);
int vfs_write(VFS_File *node, const void *buf, uint32_t size);
bool vfs_mkdir(const char *path);
bool vfs_unlink(const char *path);
Directory vfs_readdir(const char *path);
extern VFS_FS *root_fs;
VFS_Node *vfs_create_file(const char *path);
int vfs_lseek(VFS_File *node, int offset, int whence);
int vfs_close(VFS_File **pfile);
_End_C_Header
