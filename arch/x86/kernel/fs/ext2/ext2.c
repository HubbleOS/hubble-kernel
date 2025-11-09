
#include "printk.h"

#include <mm/slab.h>

#include <fs/gpt/gpt_struct.h>

#include <fs/ata/ata.h>
#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>

#include "ext2.h"
#include "ext2_struct.h"

#include <string.h>
static void ext2_read_block(EXT2_FS *fs, uint32_t block_number, void *buf)
{
	uint32_t sectors_per_block = fs->block_size / 512;
	uint32_t lba = fs->first_lba + block_number * sectors_per_block;

	for (uint32_t i = 0; i < sectors_per_block; i++)
	{
		fs->read_sector(fs->device, lba + i, ((uint8_t *)buf) + i * 512);
	}
}
static void ext2_write_block(EXT2_FS *fs, uint32_t block_number, void *buf)
{
	uint32_t sectors_per_block = fs->block_size / 512;
	uint32_t lba = fs->first_lba + block_number * sectors_per_block;

	for (uint32_t i = 0; i < sectors_per_block; i++)
	{
		fs->write_sector(fs->device, lba + i, ((uint8_t *)buf) + i * 512);
	}
}
int IS_DIR(uint16_t mode)
{
	return (mode & 0xF000) == 0x4000;
}

PathParts_ext format_folder_path_ext(const char *in)
{
	PathParts_ext result = {0};

	while (*in == '/')
		in++; // пропустити початкові '/'

	while (*in && result.count < MAX_PARTS && *in != '\0')
	{
		const char *end = in;
		while (*end && *end != '/' && *end != '\0')
			end++;

		int len = end - in;
		if (len > 0)
		{
			char name[256] = {0};
			strncpy(name, in, len);

			result.parts[result.count].name = kmalloc(len + 1);
			memcpy(result.parts[result.count].name, name, len);
			result.parts[result.count].name[len] = '\0';

			result.count++;
		}

		in = end;
		while (*in == '/' && *in != '\0')
			in++;
	}
	return result;
}

int ext2_read_group_desc(EXT2_FS *fs)
{
	// block where group descriptors start:
	uint32_t desc_block = (fs->block_size == 1024) ? 2 : 0;

	uint32_t groups_count = (fs->blocks_count + fs->blocks_per_group - 1) / fs->blocks_per_group;
	uint32_t desc_size = sizeof(Ext2GroupDesc) * groups_count;

	// скільки блоків треба, щоб прочитати всю таблицю дескрипторів:
	uint32_t blocks_needed = (desc_size + fs->block_size - 1) / fs->block_size;

	// читаємо всі блоки таблиці
	uint8_t *buf = kmalloc(blocks_needed * fs->block_size);
	if (!buf)
	{
		printk("EXT2: failed to alloc group desc buffer\n");
		return -1;
	}

	for (uint32_t i = 0; i < blocks_needed; i++)
	{
		ext2_read_block(fs, desc_block + i, buf + i * fs->block_size);
	}

	fs->groups = kmalloc(sizeof(Ext2GroupDesc) * groups_count);
	if (!fs->groups)
	{
		kfree(buf);
		printk("EXT2: failed to alloc fs->groups\n");
		return -1;
	}

	memcpy(fs->groups, buf, desc_size);
	kfree(buf);

	printk("EXT2: read %u group descriptors\n", groups_count);
	printk("inode_table: %u\n", fs->groups[0].inode_table);
	return 0;
}

int ext2_read_superblock(EXT2_FS *fs)
{
	uint8_t buf[1024];
	printk("Reading superblock\n");
	// Суперблок завжди починається через 1024 байти після початку розділу
	uint32_t superblock_lba = fs->first_lba + 2; // 1024 / 512 = 2 секторa
	printk("Superblock LBA: %u\n", superblock_lba);
	if (!fs->read_sector)
	{
		printk("EXT2: read_sector is NULL\n");
		return -1;
	}
	for (int i = 0; i < 2; i++) // читаємо 1024 байти (2×512)
		fs->read_sector(fs->device, superblock_lba + i, buf + i * 512);
	printk("EXT2: read superblock\n");
	Ext2Superblock *sb = (Ext2Superblock *)buf;

	if (sb->s_magic != 0xEF53)
	{
		printk("EXT2: invalid magic 0x%x (expected 0xEF53)\n", sb->s_magic);
		printk("first_lba=%u superblock_lba=%u\n", fs->first_lba, superblock_lba);
		return -1;
	}
	printk("EXT2: magic ok 0x%x\n", sb->s_magic);
	fs->inodes_count = sb->s_inodes_count;
	fs->blocks_count = sb->s_blocks_count;
	fs->first_data_block = sb->s_first_data_block;
	fs->log_block_size = sb->s_log_block_size;
	fs->blocks_per_group = sb->s_blocks_per_group;
	fs->inodes_per_group = sb->s_inodes_per_group;
	fs->block_size = 1024 << sb->s_log_block_size;
	fs->magic = sb->s_magic;

	fs->inode_size = (sb->s_inode_size && sb->s_inode_size >= 128) ? sb->s_inode_size : 128;

	printk("EXT2: magic ok 0x%x\n", sb->s_magic);
	printk("EXT2: block size = %u bytes\n", fs->block_size);
	printk("EXT2: inodes = %u, blocks = %u\n", fs->inodes_count, fs->blocks_count);

	return 0;
}
int ext2_read_inode(EXT2_FS *fs, uint32_t inode_number, Ext2Inode *out_inode)
{
	printk("Reading inode in func %u\n", inode_number);
	uint32_t group = (inode_number - 1) / fs->inodes_per_group;
	uint32_t index = (inode_number - 1) % fs->inodes_per_group;
	printk("group=%u index=%u\n", group, index);

	printk("inode_table=%u\n", fs->groups[0].inode_table);
	printk("inode_size=%u\n", fs->inode_size);
	Ext2GroupDesc *gd = &fs->groups[group];
	uint32_t inode_table_block = gd->inode_table;

	if (inode_table_block == 0)
	{
		printk("inode_table_block == 0\n");
		return -1;
	}
	printk("inode_table_block=%u\n", inode_table_block);

	uint32_t inode_size = fs->inode_size;
	uint32_t offset = index * inode_size;

	uint32_t block_offset = offset / fs->block_size;
	uint32_t offset_in_block = offset % fs->block_size;

	uint8_t *block_buf = kmalloc(fs->block_size);
	if (!block_buf)
		return -1;
	printk("inode_table_block=%u block_offset=%u offset_in_block=%u\n", inode_table_block, block_offset, offset_in_block);
	ext2_read_block(fs, inode_table_block + block_offset, block_buf);
	printk("inode_size=%u\n", inode_size);
	memcpy(out_inode, block_buf + offset_in_block, sizeof(Ext2Inode)); // переконайсь, що sizeof(Ext2Inode) <= inode_size
	printk("inode read\n");
	kfree(block_buf);
	return 0;
}

Directory ext2_list_dir(EXT2_FS *fs, Ext2Inode *dir_inode)
{
	if (!IS_DIR(dir_inode->mode))
	{
		printk("Not a directory\n");
		return Directory_init((Directory){.entries = NULL, .count = 0});
	}

	uint8_t *block_buf = kmalloc(fs->block_size);

	printk("Listing directory (size=%u bytes)\n", dir_inode->size);

	Directory dir = Directory_init((Directory){.entries = kmalloc(1024), .count = 0});

	for (int i = 0; i < 12 && dir_inode->block[i]; i++) // тільки прямі блоки для простої версії
	{
		printk("read in for");
		ext2_read_block(fs, dir_inode->block[i], block_buf);
		printk("test");
		uint32_t offset = 0;
		while (offset < fs->block_size)
		{
			Ext2DirEntry *entry = (Ext2DirEntry *)(block_buf + offset);

			if (entry->inode == 0)
				break;
			dir.entries[dir.count].name = kmalloc(entry->name_len + 1);
			if (!dir.entries[dir.count].name)
			{
				printk("failed");
			}
			printk("teto 2");
			memcpy(dir.entries[dir.count].name, entry->name, entry->name_len);
			printk("teto3 count - %d\n", dir.count);
			dir.entries[dir.count]
				.name[entry->name_len] = '\0';
			dir.entries[dir.count].is_dir = entry->file_type == 0x10;
			dir.entries[dir.count].cluster = entry->inode;
			dir.count++;

			// printk(" - %s (inode=%u, type=%u)\n", name, entry->inode, entry->file_type);

			offset += entry->rec_len;
			if (entry->rec_len == 0)
			{
				break;
			}
		}
	}
	printk("teto ultima");
	kfree(block_buf);
	return dir;
}
uint32_t ext2_allocate_inode(EXT2_FS *fs, uint32_t group)
{
	uint8_t *bitmap = kmalloc(fs->block_size);
	ext2_read_block(fs, fs->groups[group].inode_bitmap, bitmap);

	for (uint32_t i = 0; i < fs->inodes_per_group; i++)
	{
		uint32_t byte = i / 8;
		uint8_t bit = 1 << (i % 8);
		if (!(bitmap[byte] & bit))
		{
			bitmap[byte] |= bit; // помічаємо inode як зайнятий
			ext2_write_block(fs, fs->groups[group].inode_bitmap, bitmap);
			kfree(bitmap);
			return i + 1 + group * fs->inodes_per_group; // глобальний номер inode
		}
	}

	kfree(bitmap);
	return 0; // немає вільних inode
}

uint32_t ext2_allocate_block(EXT2_FS *fs, uint32_t group)
{
	uint8_t *bitmap = kmalloc(fs->block_size);
	ext2_read_block(fs, fs->groups[group].block_bitmap, bitmap);

	for (uint32_t i = 0; i < fs->blocks_per_group; i++)
	{
		uint32_t byte = i / 8;
		uint8_t bit = 1 << (i % 8);
		if (!(bitmap[byte] & bit))
		{
			bitmap[byte] |= bit;
			ext2_write_block(fs, fs->groups[group].block_bitmap, bitmap);
			kfree(bitmap);
			return fs->first_data_block + i + group * fs->blocks_per_group;
		}
	}

	kfree(bitmap);
	return 0;
}

int ext2_write_inode(EXT2_FS *fs, uint32_t inode_num, Ext2Inode *inode)
{
	uint32_t group = (inode_num - 1) / fs->inodes_per_group;
	uint32_t index = (inode_num - 1) % fs->inodes_per_group;
	uint32_t table_block = fs->groups[group].inode_table;

	uint32_t inode_size = sizeof(Ext2Inode);
	uint32_t inodes_per_block = fs->block_size / inode_size;
	uint32_t block_offset = index / inodes_per_block;
	uint32_t offset = index % inodes_per_block;

	uint8_t *buf = kmalloc(fs->block_size);
	ext2_read_block(fs, table_block + block_offset, buf);

	memcpy(buf + offset * inode_size, inode, inode_size);
	ext2_write_block(fs, table_block + block_offset, buf);

	kfree(buf);
	return 0;
}
int ext2_add_dir_entry(EXT2_FS *fs, uint32_t dir_inode_num, const char *name, uint32_t inode_num, uint8_t file_type)
{
	Ext2Inode dir_inode;
	ext2_read_inode(fs, dir_inode_num, &dir_inode);

	uint8_t *block = kmalloc(fs->block_size);
	ext2_read_block(fs, dir_inode.block[0], block);

	uint32_t offset = 0;
	while (offset < fs->block_size)
	{
		Ext2DirEntry *entry = (Ext2DirEntry *)(block + offset);
		if (offset + entry->rec_len >= fs->block_size)
		{
			uint16_t actual_len = 8 + ((entry->name_len + 3) & ~3);
			uint16_t new_len = fs->block_size - offset - actual_len;
			entry->rec_len = actual_len;

			Ext2DirEntry *new_entry = (Ext2DirEntry *)(block + offset + actual_len);
			new_entry->inode = inode_num;
			new_entry->rec_len = new_len;
			new_entry->name_len = strlen(name);
			new_entry->file_type = file_type;
			memcpy(new_entry->name, name, new_entry->name_len);

			ext2_write_block(fs, dir_inode.block[0], block);
			kfree(block);
			return 0;
		}
		offset += entry->rec_len;
	}

	kfree(block);
	return -1;
}
uint32_t ext2_create_file(EXT2_FS *fs, uint32_t parent_inode, const char *name)
{
	uint32_t new_inode = ext2_allocate_inode(fs, 0);
	uint32_t new_block = ext2_allocate_block(fs, 0);

	Ext2Inode inode = {0};
	inode.mode = 0x8000 | 0644; // звичайний файл
	inode.size = 0;
	inode.blocks = 2; // у 512-блоках
	inode.block[0] = new_block;

	ext2_write_inode(fs, new_inode, &inode);
	ext2_add_dir_entry(fs, parent_inode, name, new_inode, 1); // 1 = файл

	return new_inode;
}

int ext2_init(EXT2_FS *fs)
{
	printk("Initializing EXT2\n");
	printk("Reading superblock\n");
	if (ext2_read_superblock(fs) < 0)
		return -1;
	printk("Reading group descriptors\n");
	if (ext2_read_group_desc(fs) < 0)
		return -1;
	Ext2Inode root_inode;
	printk("Reading root inode\n");
	ext2_read_inode(fs, 2, &root_inode);

	printk("Root inode size = %u bytes\n", root_inode.size);
	printk("Root inode first block = %u\n", root_inode.block[0]);
	// display all info
	printk("inode blocks count = %u\n", root_inode.blocks);
	ext2_list_dir(fs, &root_inode);
	return 0;
}
uint32_t ext2_find_dir_entry(EXT2_FS *fs, uint32_t inode, const char *name)
{
	Ext2Inode dir_inode;
	ext2_read_inode(fs, inode, &dir_inode);

	uint8_t *block_buf = kmalloc(fs->block_size);
	ext2_read_block(fs, dir_inode.block[0], block_buf);

	uint32_t offset = 0;
	while (offset < fs->block_size)
	{
		Ext2DirEntry *entry = (Ext2DirEntry *)(block_buf + offset);
		if (entry->inode == 0)
			break;
		if (entry->file_type == 1 && strcmp(entry->name, name) == 0)
		{
			kfree(block_buf);
			return entry->inode;
		}
		offset += entry->rec_len;
	}
	kfree(block_buf);
	return 0;
}
uint32_t ext2_parse_path(EXT2_FS *fs, uint32_t inode, const char *path)
{
	PathParts_ext parts = format_folder_path_ext(path);
	for (int i = 0; i < parts.count; i++)
	{
		inode = ext2_find_dir_entry(fs, inode, parts.parts[i].name);
		if (inode == 0)
			return 0;
	}
	return inode;
}

// void ext2_init(gpt_partition_t part)
// {
// 	printk("Initializing EXT2 on partition starting at LBA %lu\n\n", part.first_lba);

// 	VFS_Device *device = part.device;

// 	uint8_t *buf = kmalloc(1024 * 2);

// 	device->read(device->device, part.first_lba, buf);
// 	device->read(device->device, part.first_lba + 1, buf + 512);
// 	device->read(device->device, part.first_lba + 2, buf + 1024);

// 	Ext2Superblock *superblock = (Ext2Superblock *)(buf + 1024);

// 	if (superblock->s_magic != 0xEF53)
// 	{
// 		printk("Superblock magic number: 0x%x (invalid)\n", superblock->s_magic);
// 		return;
// 	}

// 	printk("EXT2 Superblock OK (magic 0x%x)\n", superblock->s_magic);
// 	printk("Block size: %u\n", 1024 << superblock->s_log_block_size);
// 	printk("Inodes count: %u\n", superblock->s_inodes_count);
// 	printk("Blocks count: %u\n", superblock->s_blocks_count);
// 	printk("Blocks per group: %u\n", superblock->s_blocks_per_group);
// 	printk("Inodes per group: %u\n", superblock->s_inodes_per_group);
// 	printk("First data block: %u\n", superblock->s_first_data_block);
// 	printk("Volume name: %.16s\n", superblock->s_volume_name);

// 	EXT2_FS *fs = kmalloc(sizeof(EXT2_FS));
// 	if (!fs)
// 	{
// 		printk("Failed to allocate EXT2_FS\n");
// 		return;
// 	}

// 	fs->device = device->device;
// 	fs->start_lba = part.first_lba;
// 	fs->read_sector = device->read;
// 	fs->write_sector = device->write;

// 	fs->block_size = 1024 << superblock->s_log_block_size;
// 	fs->blocks_per_group = superblock->s_blocks_per_group;
// 	fs->inodes_per_group = superblock->s_inodes_per_group;
// 	fs->first_data_block = superblock->s_first_data_block;
// 	fs->blocks_count = superblock->s_blocks_count;
// 	fs->inodes_count = superblock->s_inodes_count;

// 	uint32_t group_desc_lba = (fs->first_data_block + 1) * (fs->block_size / 512);

// 	uint8_t *buffer = kmalloc(fs->block_size);

// 	device->read(device->device, group_desc_lba, buffer);

// 	for (int i = 0; i < fs->inodes_per_group; i++)
// 	{
// 		Ext2Inode *inode = (Ext2Inode *)(buffer + i * sizeof(Ext2Inode));

// 		if (inode->i_mode == 0)
// 		{
// 			continue;
// 		}

// 		fs->inodes[i] = inode;
// 	}

// 	kfree(buf);
// }
