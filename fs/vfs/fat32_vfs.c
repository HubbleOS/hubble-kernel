#include "vfs.h"
#include "vfs_standart_struct.h"
#include "printk.h"

#include <fs/fat32/fat.h>
#include <mm/kmalloc.h>

#include <hubble/string.h>

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN 0x02
#define FAT32_ATTR_SYSTEM 0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10

static bool fat32_mount_wrapper(VFS_FS *fs, VFS_Device *device, uint32_t start_lba)
{
	fs->fs = kmalloc(sizeof(FAT32_FS), GFP_KERNEL);
	return fat32_mount(fs->fs, device, start_lba);
}

static void fat32_unmount_wrapper(VFS_FS *fs)
{
	fat32_unmount(fs->fs);
}

static VFS_Node *fat32_open_wrapper(VFS_FS *fs, const char *path)
{
	if (!fs->fs)
	{
		printk("fs not mounted\n");
		return NULL;
	}
	printk("Opening file: %s\n", path);
	FAT32_File *file = fat32_open(fs->fs, path);
	printk("\nEntry open here!!: ");
	// for (int i = 0; i < sizeof(FAT32_DirectoryEntry); i++)
	// {
	// 	printk("%c", file->entry[i]);
	// }
	if (!file)
	{
		printk("\nFailed to open file\n");
		return NULL;
	}

	VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
	memset(node, 0, sizeof(VFS_Node));
	strncpy(node->name, path, 255);
	node->is_dir = file->entry->attr & 0x10;
	node->size = file->entry->file_size;
	node->fs_node = file;
	node->fs = fs;
	if (file->entry->attr & FAT32_ATTR_DIRECTORY)
	{
		node->mode |= MODE_DIR;
	}
	else
	{
		node->mode |= MODE_FILE;
	}

	if (file->entry->attr & FAT32_ATTR_READ_ONLY)
	{
		node->mode |= MODE_READ;
	}
	else
	{
		node->mode |= MODE_READ | MODE_WRITE;
	}
	printk("Entry open: %s\n", node->name);
	return node;
}

static int fat32_read_wrapper(VFS_File *node, void *buf, uint32_t size)
{
	memset(buf, 0, size);
	return fat32_read(node, buf, size);
}

static int fat32_write_wrapper(VFS_File *node, const void *buf, uint32_t size)
{
	return fat32_write(node, buf, size);
}
static VFS_Node *fat32_create_file_wrapper(VFS_FS *fs, const char *path)
{
	printk("Creating file: %s\n", path);

	if (!fs->fs)
	{
		printk("fs not mounted\n");
		return NULL;
	}

	fat32_create_file((FAT32_FS *)(fs->fs), path);
	return fat32_open_wrapper(fs, path);
}
static bool fat32_mkdir_wrapper(VFS_FS *fs, const char *path)
{
	return fat32_mkdir((FAT32_FS *)(fs->fs), path);
}

static bool fat32_unlink_wrapper(VFS_FS *fs, const char *path)
{
	return fat32_delete(((FAT32_FS *)(fs - fs)), path);
}
static Directory fat32_readdir_wrapper(VFS_FS *fs, const char *path)
{
	return fat32_list_files_from_path((FAT32_FS *)(fs->fs), path);
}
static int fat32_close_wrapper(VFS_File *file)
{
	if (!file)
		return -1;

	VFS_Node *node = file->node;
	const char *name = node && node->name ? node->name : "<unknown>";

	printk("VFS: closing file %s\n", name);
	if (node)
	{
		if (node->fs_node)
			kfree(node->fs_node);
		kfree(node);
	}
	kfree(file);

	printk("VFS: file was closed %s\n", name);
	return 0;
}

void fat32_init_vfs(VFS_FS *fs)
{
	fs->mount = fat32_mount_wrapper;
	fs->unmount = fat32_unmount_wrapper;
	fs->open = fat32_open_wrapper;
	fs->read = fat32_read_wrapper;
	fs->write = fat32_write_wrapper;
	fs->create_file = fat32_create_file_wrapper;
	fs->mkdir = fat32_mkdir_wrapper;
	fs->unlink = fat32_unlink_wrapper;
	fs->readdir = fat32_readdir_wrapper;
	fs->close = fat32_close_wrapper;
}
