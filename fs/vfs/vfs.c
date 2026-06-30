/* ── Virtual Filesystem (VFS) core implementation ─────────────────
 * Implements mount management, path resolution across mountpoints,
 * and the public VFS API (open, read, write, mkdir, readdir, etc.).
 * ────────────────────────────────────────────────────────────────── */

#include "vfs.h"
#include "vfs_standart_struct.h"

#include <hubble/string.h>
#include <stdarg.h>
#include <hubble/errno.h>
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

/** @brief Get the relative path portion after a mountpoint prefix. */
static const char *vfs_get_relpath(const char *path, const char *mountpoint)
{
	size_t mlen = strlen(mountpoint);

	if (strcmp(path, mountpoint) == 0)
		return "/";

	if (strncmp(path, mountpoint, mlen) == 0)
	{
		const char *rel = path + mlen;
		if (*rel == '/')
			rel++;
		return rel;
	}

	return NULL;
}

/** @brief Mount a filesystem at the given mountpoint. */
bool vfs_mount(const char *mountpoint, gpt_partition_t *partition, FileSystemType type)
{
	VFS_FS *fs = kmalloc(sizeof(VFS_FS), GFP_KERNEL);
	memset(fs, 0, sizeof(VFS_FS));
	fs->type = type;

	switch (type)
	{
	case FS_FAT32:
		printk(KERN_INFO "VFS: init fat32 vfs\n");
		extern void fat32_init_vfs(VFS_FS * fs);
		fat32_init_vfs(fs);
		break;
	case FS_EXT2:
		printk(KERN_INFO "VFS: init ext2 vfs\n");
		extern void ext2_init_vfs(VFS_FS * fs);
		ext2_init_vfs(fs);
		break;
	case FS_DEV:
		printk(KERN_INFO "VFS: init dev vfs\n");
		extern void dev_vfs_init(VFS_FS * fs, VFS_Device * device, uint32_t start_lba);
		dev_vfs_init(fs, partition->device, partition->first_lba);
		break;
	case FS_PIPE:
		printk(KERN_INFO "VFS: init pipe vfs\n");
		extern void pipe_vfs_init(VFS_FS * fs, VFS_Device * device, uint32_t start_lba);
		pipe_vfs_init(fs, partition->device, partition->first_lba);
		break;
	default:
		printk(KERN_INFO "VFS: unsupported FS type %d\n", type);
		kfree(fs);
		return false;
	}
	printk(KERN_INFO "VFS: mount %d at %s\n", type, mountpoint);
	if (!partition->device->read && (type != FS_DEV || type != FS_PIPE))
		printk(KERN_INFO "partition has no device\n");

	printk(KERN_INFO "VFS: mount %d at %s\n", type, mountpoint);

	if ((type != FS_DEV && type != FS_PIPE))
	{
		if (!fs->mount(fs, partition->device, partition->first_lba))
		{
			printk(KERN_ERR "VFS: failed to mount %d at %s\n", type, mountpoint);
			return false;
		}
	}
	printk(KERN_INFO "VFS: mounted %d at %s\n", type, mountpoint);
	if (!fs->fs)
		printk(KERN_ERR "VFS: failed to mount %d at %s\n", type, mountpoint);

	VFS_Mount *mnt = kmalloc(sizeof(VFS_Mount), GFP_KERNEL);
	strcpy(mnt->mountpoint, mountpoint);
	mnt->fs = fs;
	mnt->next = vfs_mounts;
	vfs_mounts = mnt;

	printk(KERN_INFO "VFS: mounted %d at %s\n", type, mountpoint);
	return true;
}

/** @brief Find the deepest mountpoint matching a path. */
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
			{
				best = m;
				best_len = len;
			}
		}
	}
	return best;
}

/** @brief Open a file by path with the given flags. */
VFS_File *vfs_open(const char *path, int flags)
{
	printk(KERN_INFO "VFS: opening file %s\n", path);
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	if (!mnt)
		return ERR_PTR(-ENOENT);

	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (*relpath == '/')
		relpath++;

	printk(KERN_INFO "VFS: opening file %s\n", relpath);
	VFS_Node *node = mnt->fs->open(mnt->fs, relpath);
	printk(KERN_INFO "after open\n");

	if (!node && (flags & VFS_O_CREAT))
		node = mnt->fs->create_file(mnt->fs, relpath);

	if (!node)
		return ERR_PTR(-ENOENT);

	printk(KERN_INFO "node pointer: %p\n", node);

	VFS_File *f = kmalloc(sizeof(VFS_File), GFP_KERNEL);
	f->node = node;
	f->flags = flags;
	f->pos = 0;
	return f;
}

/** @brief Read from an open VFS file. */
int vfs_read(VFS_File *file, void *buf, uint32_t size)
{
	if (!file || !file->node->fs || !file->node->fs->read)
	{
		printk(KERN_ERR "VFS: read error\n");
		return -EIO;
	}
	if (file->pos >= file->node->size)
	{
		printk(KERN_INFO "VFS: EOF\n");
		return 0;
	}
	return file->node->fs->read(file, buf, size);
}

/** @brief Write to an open VFS file. */
int vfs_write(VFS_File *file, const void *buf, uint32_t size)
{
	if (!file || !file->node->fs || !file->node->fs->write)
		return -EIO;
	return file->node->fs->write(file, buf, size);
}

/** @brief Create a file (returns VFS_Node). */
VFS_Node *vfs_create_file(const char *path)
{
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt || !mnt->fs || !mnt->fs->create_file)
	{
		printk(KERN_ERR "VFS: failed to create file %s\n", path);
		return NULL;
	}
	return mnt->fs->create_file(mnt->fs, relpath);
}

/** @brief Create a directory. */
bool vfs_mkdir(const char *path)
{
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt)
		return false;
	return mnt->fs->mkdir(mnt->fs, relpath);
}

/** @brief Read a directory listing. */
Directory vfs_readdir(const char *path)
{
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt)
		return (Directory){0};

	return mnt->fs->readdir(mnt->fs, relpath);
}

/** @brief Unlink (delete) a file or directory. */
bool vfs_unlink(const char *path)
{
	VFS_Mount *mnt = vfs_find_mount_for_path(path);
	const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
	if (!mnt)
		return false;
	return mnt->fs->unlink(mnt->fs, relpath);
}

/** @brief Seek to a position in an open file. */
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

/** @brief Close an open file and free allocated resources. */
int vfs_close(VFS_File *file)
{
	if (!file || !file->node || !file->node->fs || !file->node->fs->close)
		return -EIO;

	file->node->fs->close(file);

	return 0;
}
