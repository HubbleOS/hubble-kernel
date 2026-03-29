#include "vfs.h"
#include "vfs_standart_struct.h"
#include "printk.h"

#include <fs/ext2/ext2.h>
#include <fs/ext2/ext2_struct.h>
#include <mm/kmalloc.h>

#include <hubble/string.h>

#define EXT2_ATTR_READ_ONLY 0x01
#define EXT2_ATTR_HIDDEN 0x02
#define EXT2_ATTR_SYSTEM 0x04
#define EXT2_ATTR_VOLUME_ID 0x08
#define EXT2_ATTR_DIRECTORY 0x10
#define EXT2_ATTR_ARCHIVE 0x20

#define EXT2_BLOCK_SIZE 1024

bool ext2_vfs_init(VFS_FS *fs, VFS_Device *device, uint32_t start_lba)
{
	printk("Initializing device\n");
	EXT2_FS *ext2_fs = kmalloc(sizeof(EXT2_FS), GFP_KERNEL);
	ext2_fs->device = device->device;
	ext2_fs->read_sector = device->read;
	if (!ext2_fs->read_sector)
	{
		if (device->read == NULL)
		{
			printk("EXT2: read_sector is NULL from param\n");
		}
		printk("EXT2: read_sector is NULL\n");
		return 0;
	}
	ext2_fs->write_sector = device->write;
	ext2_fs->first_lba = start_lba;
	printk("Initializing EXT2 on partition starting at LBA %u\n", start_lba);
	ext2_init(ext2_fs);
	printk("EXT2 Superblock OK (magic 0x%x)\n", ext2_fs->magic);
	fs->fs = ext2_fs;
	return 1;
}

static VFS_Node *ext2_vfs_open(VFS_FS *fs, const char *path)
{
	return NULL;
}

int ext2_vfs_read(VFS_File *node, void *buffer, uint32_t size)
{
	return 0;
}

int ext2_vfs_write(VFS_File *node, const void *buffer, uint32_t size)
{
	return 0;
}

int ext2_vfs_close(VFS_File *file)
{
	return 0;
}

int ext2_vfs_lseek(VFS_File *node, int offset, int whence)
{
	return 0;
}

bool ext2_vfs_unlink(VFS_FS *fs, const char *path)
{
	return 0;
}

bool ext2_vfs_mkdir(VFS_FS *fs, const char *path)
{
	return 0;
}

Directory ext2_vfs_readdir(VFS_FS *fs, const char *path)
{

	printk("path: %s\n", path);
	Ext2Inode *inode = kmalloc(sizeof(Ext2Inode), GFP_KERNEL);
	// inode = ext2_find_dir_entry(fs->fs, 2, path);
	ext2_read_inode(fs->fs, ext2_parse_path(fs->fs, 2, path), inode);
	return ext2_list_dir(fs->fs, inode);
}

// void fat32_init_vfs(VFS_FS *fs)
// {
// 	fs->mount = fat32_mount_wrapper;
// 	fs->unmount = fat32_unmount_wrapper;
// 	fs->open = fat32_open_wrapper;
// 	fs->read = fat32_read_wrapper;
// 	fs->write = fat32_write_wrapper;
// 	fs->create_file = fat32_create_file_wrapper;
// 	fs->mkdir = fat32_mkdir_wrapper;
// 	fs->unlink = fat32_unlink_wrapper;
// 	fs->readdir = fat32_readdir_wrapper;
// 	fs->close = fat32_close_wrapper;
// }

void ext2_init_vfs(VFS_FS *fs)
{
	fs->mount = ext2_vfs_init;
	fs->open = ext2_vfs_open;
	fs->read = ext2_vfs_read;
	fs->write = ext2_vfs_write;
	fs->close = ext2_vfs_close;
	fs->unlink = ext2_vfs_unlink;
	fs->mkdir = ext2_vfs_mkdir;
	fs->readdir = ext2_vfs_readdir;
}
