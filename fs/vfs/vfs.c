#include "vfs.h"
#include "vfs_standart_struct.h"

#include <hubble/string.h>
#include <stdarg.h>
#include <hubble/errno.h>
// #include "gpt.h" // Список змонтованих ФС (поки що 1)
#include <fs/gpt/gpt.h>
#include <mm/kmalloc.h>
#include <hubble/printk.h>

typedef struct VFS_Mount
{
	char mountpoint[10];
	VFS_FS *fs;
	struct VFS_Mount *next;
} VFS_Mount;

static VFS_Mount *vfs_mounts = NULL;

static const char *vfs_get_relpath(const char *path, const char *mountpoint)
{
	size_t mlen = strlen(mountpoint);

	// Якщо шлях точно дорівнює mountpoint
	if (strcmp(path, mountpoint) == 0)
		return "/"; // корінь цієї ФС

	// Якщо шлях починається з точки монтування
	if (strncmp(path, mountpoint, mlen) == 0)
	{
		const char *rel = path + mlen;
		if (*rel == '/')
			rel++; // пропускаємо зайву '/'
		return rel;
	}

	// Інакше — це не цей mount
	return NULL;
}

bool vfs_mount(const char *mountpoint, gpt_partition_t *partition, FileSystemType type)
{
	VFS_FS *fs = kmalloc(sizeof(VFS_FS), GFP_KERNEL);
	memset(fs, 0, sizeof(VFS_FS));
	fs->type = type;

	switch (type)
	{
	case FS_FAT32:
		printk("VFS: init fat32 vfs\n");
		extern void fat32_init_vfs(VFS_FS * fs);
		fat32_init_vfs(fs);
		break;
	case FS_EXT2:
		printk("VFS: init ext2 vfs\n");
		extern void ext2_init_vfs(VFS_FS * fs);
		ext2_init_vfs(fs);
		break;
	case FS_DEV:
		printk("VFS: init dev vfs\n");
		extern void dev_vfs_init(VFS_FS * fs, VFS_Device * device, uint32_t start_lba);
		dev_vfs_init(fs, partition->device, partition->first_lba);
		break;
	default:
		printk("VFS: unsupported FS type %d\n", type);
		kfree(fs);
		return false;
	}
	printk("VFS: mount %d at %s\n", type, mountpoint);
	if (!partition->device->read && type != FS_DEV)
	{
		printk("partition has no device\n");
	}
	printk("VFS: mount %d at %s\n", type, mountpoint);
	if (type != FS_DEV && !fs->mount(fs, partition->device, partition->first_lba))
	{
		printk("VFS: failed to mount %d at %s\n", type, mountpoint);
		return false;
	}
	if (!fs->fs)
	{
		printk("VFS: failed to mount %d at %s\n", type, mountpoint);
	}
	VFS_Mount *mnt = kmalloc(sizeof(VFS_Mount), GFP_KERNEL);
	strcpy(mnt->mountpoint, mountpoint);
	mnt->fs = fs;
	mnt->next = vfs_mounts;
	vfs_mounts = mnt;

	printk("VFS: mounted %d at %s\n", type, mountpoint);
	return true;
}

static VFS_Mount *vfs_find_mount_for_path(const char *path)
{
	VFS_Mount *best = NULL;
	size_t best_len = 0;

	for (VFS_Mount *m = vfs_mounts; m; m = m->next)
	{
		size_t len = strlen(m->mountpoint);
		if (strncmp(path, m->mountpoint, len) == 0)
		{
			if (len > best_len)
			{ // найглибший збіг
				best = m;
				best_len = len;
			}
		}
	}
	return best;
}

VFS_File *vfs_open(const char *path, int flags)
{
	printk("VFS: opening file %s\n", path);
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	if (!mnt)
		return ERR_PTR(-ENOENT);

	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (*relpath == '/')
		relpath++;

	printk("VFS: opening file %s\n", relpath);
	VFS_Node *node = mnt->fs->open(mnt->fs, relpath);
	printk("after open\n");

	if (!node && (flags & VFS_O_CREAT))
		node = mnt->fs->create_file(mnt->fs, relpath);

	if (!node)
		return ERR_PTR(-ENOENT);
	printk("node pointer: %p\n", node);

	VFS_File *f = kmalloc(sizeof(VFS_File), GFP_KERNEL);
	f->node = node;
	f->flags = flags;
	f->pos = 0;
	return f;
}

int vfs_read(VFS_File *file, void *buf, uint32_t size)
{
	if (!file || !file->node->fs || !file->node->fs->read)
	{
		printk("VFS: read error\n");
		return -EIO;
	}
	if (file->pos >= file->node->size)
	{
		printk("VFS: EOF\n");
		return 0;
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
	// if (!root_fs || !root_fs->create_file)
	// 	return NULL;
	// return root_fs->create_file(root_fs, path);
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt || !mnt->fs || !mnt->fs->create_file)
	{
		printk("VFS: failed to create file %s\n", path);
		return NULL;
	}
	return mnt->fs->create_file(mnt->fs, relpath);
}

bool vfs_mkdir(const char *path)
{
	// if (!root_fs || !root_fs->mkdir)
	// 	return false;
	// return root_fs->mkdir(root_fs, path);
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt)
		return false;
	return mnt->fs->mkdir(mnt->fs, relpath);
}

Directory vfs_readdir(const char *path)
{

	// if (!root_fs || !root_fs->readdir)
	// 	return (Directory){0};
	// printk("VFS: reading directory %s\n", path);
	// return root_fs->readdir(root_fs, path);
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt)
	{
		return (Directory){0};
	}
	return mnt->fs->readdir(mnt->fs, relpath);
}

bool vfs_unlink(const char *path)
{
	// if (!root_fs || !root_fs->unlink)
	// 	return false;
	// return root_fs->unlink(root_fs, path);
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt)
		return false;
	return mnt->fs->unlink(mnt->fs, relpath);
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

// int vfs_close(VFS_File **pfile)
// {
// 	VFS_File *file = *pfile;
// 	if (!file || !file->node->fs || !file->node->fs->close)
// 		return -EIO;
// 	file->node->fs->close(file);
// 	// *pfile = NULL;
// 	if (file)
// 	{
// 		// printk("%s\n", file->node->name);
// 		printk("VFS: file was not closed in vfs %s\n", file->node->name);
// 	}
// 	return 0;
// }

int vfs_close(VFS_File *file)
{
	if (!file || !file->node || !file->node->fs || !file->node->fs->close)
		return -EIO;

	file->node->fs->close(file);

	return 0;
}
