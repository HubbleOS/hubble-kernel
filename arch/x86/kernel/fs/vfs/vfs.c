#include "vfs.h"
#include "vfs_standart_struct.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
// #include "gpt.h" // Список змонтованих ФС (поки що 1)
#include <fs/gpt/gpt.h>
VFS_FS *root_fs = NULL;

// ==== Реалізація VFS API ==== //

bool vfs_mount(gpt_partition_t *parition, FileSystemType type)
{
	if (root_fs != NULL)
	{
		printf("VFS: already mounted\n");
		return false;
	}
	printf("VFS: mounting\n");
	root_fs = malloc(sizeof(VFS_FS));
	memset(root_fs, 0, sizeof(VFS_FS));
	root_fs->type = type;

	// вибір драйвера
	switch (type)
	{
	case FS_FAT32:
		extern void fat32_init_vfs(VFS_FS * fs); // функція з fat32_vfs.c
		fat32_init_vfs(root_fs);
		break;
	default:
		printf("VFS: unsupported FS type %d\n", type);
		free(root_fs);
		root_fs = NULL;
		return false;
	}
	printf("VFS: mounted\n");
	return root_fs->mount(root_fs, parition->device, parition->first_lba);
}

VFS_File *vfs_open(const char *path, int flags)

{

	if (!root_fs || !root_fs->open)
		return ERR_PTR(-ENODEV);
	VFS_Node *node = root_fs->open(root_fs, path);

	VFS_File *f = malloc(sizeof(VFS_File));

	printf("VFS: opening file %s\n", path);

	if (!node)
	{
		printf("VFS: file %s not found\n", path);

		if (flags & VFS_O_CREAT)
		{
			printf("VFS: creating file %s\n", path);
			node = vfs_create_file(path);
			if (!node)
			{
				// f->flags = -EIO;
				return ERR_PTR(-EIO);
			}
		}
		else
		{
			// f->flags = -ENOENT;
			return ERR_PTR(-ENOENT);
		}
	}
	else
	{
		// Якщо файл вже існує
		if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL))
		{
			// return NULL;
			return ERR_PTR(-EEXIST); // існує, а ми хочемо створити з EXCL
		}
	}
	printf("VFS: file opened %s\n", path);
	// --- перевірка режимів ---
	int access_mode = flags & 0x03; // беремо тільки нижні біти
	switch (access_mode)
	{
	case VFS_O_RDONLY:
		if (!(node->mode & MODE_READ))
			return ERR_PTR(-EACCES);
		break;
	case VFS_O_WRONLY:
		if (!(node->mode & MODE_WRITE))
			return ERR_PTR(-EACCES);
		break;
	case VFS_O_RDWR:
		if (!(node->mode & MODE_READ) || !(node->mode & MODE_WRITE))
			return ERR_PTR(-EACCES);
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	// --- trunc ---
	// if ((flags & VFS_O_TRUNC) && (access_mode != VFS_O_RDONLY))
	//{
	//    node->size = 0;
	//    fs_truncate(node); // драйвер FS реально обрізає
	//}

	// --- append ---

	f->node = node;
	f->flags = flags;
	f->pos = (flags & VFS_O_APPEND) ? node->size : 0;
	printf("VFS: file opened %s\n", path);
	return f;
}

int vfs_read(VFS_File *file, void *buf, uint32_t size)
{
	if (!file || !file->node->fs || !file->node->fs->read)
	{
		printf("VFS: read error\n");
		return -EIO;
	}
	if (file->pos >= file->node->size)
	{
		printf("VFS: EOF\n");
		return -0;
	}
	return file->node->fs->read(file, buf, size);
}

int vfs_write(VFS_File *file, const void *buf, uint32_t size)
{
	if (!file || !file->node->fs || !file->node->fs->write)
		return -EIO;
	return file->node->fs->write(file, buf, size);
}

VFS_Node *vfs_create_file(const char *path)
{
	if (!root_fs || !root_fs->create_file)
		return NULL;
	return root_fs->create_file(root_fs, path);
}

bool vfs_mkdir(const char *path)
{
	if (!root_fs || !root_fs->mkdir)
		return false;
	return root_fs->mkdir(root_fs, path);
}
Directory vfs_readdir(const char *path)
{

	if (!root_fs || !root_fs->readdir)
		return (Directory){0};
	printf("VFS: reading directory %s\n", path);
	return root_fs->readdir(root_fs, path);
}

bool vfs_unlink(const char *path)
{
	if (!root_fs || !root_fs->unlink)
		return false;
	return root_fs->unlink(root_fs, path);
}
int vfs_lseek(VFS_File *file, int offset, int whence)
{
	if (!file || !file->node->fs)
		return -1;

	switch (whence)
	{
	case SEEK_SET:
		file->pos = offset;
		break;
	case SEEK_CUR:
		file->pos += offset;
		break;
	case SEEK_END:
		file->pos = file->node->size + offset;
		break;
	default:
		return -1;
	}

	return 0;
}
