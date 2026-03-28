#include "fat.h"
#include "printk.h"

#include "fat_structs.h"
#include "fat_utils.h"
#include <fs/vfs/vfs_standart_struct.h>

#include <mm/kmalloc.h>

#include <drivers/storage/ata/ata.h>

#include <string.h>
#include <stdbool.h>

void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr)
{
	size_t namelen = strlen(name);
	size_t need = namelen + 2; // prefix + '\n'
	ctx_ptr->entries[ctx_ptr->count].name = kmalloc(need, GFP_KERNEL);
	if (!ctx_ptr->entries[ctx_ptr->count].name)
		return;
	memset(ctx_ptr->entries[ctx_ptr->count].name, 0, need);
	memcpy(ctx_ptr->entries[ctx_ptr->count].name, name, namelen);
	ctx_ptr->entries[ctx_ptr->count].is_dir = is_dir;
	ctx_ptr->count++;
}

Directory fat32_list_files(FAT32_FS *fs, uint32_t cluster)
{
	Directory ctx = Directory_init((Directory){.entries = kmalloc(1024, GFP_KERNEL), .count = 0});
	if (!ctx.entries)
	{
		printk("Failed to allocate directory\n");
	}
	iterate_directory(fs, cluster, list_files_callback, &ctx);
	printk("count: %d\n", ctx.count);
	return ctx;
}

Directory fat32_list_files_from_path(FAT32_FS *fs, const char *path)
{

	if (!path)
	{
		printk("Path not found: %s\n", path);
		return (Directory){.entries = NULL, .count = 0};
	}

	uint32_t cluster = resolve_path_to_cluster(fs, path);
	printk("cluster show: %d\n", cluster);
	if (cluster == 0)
	{
		printk("Path not found: %s\n", path);
		return (Directory){.entries = NULL, .count = 0};
	}
	printk("cluster: %d\n", cluster);
	return fat32_list_files(fs, cluster);
}

uint32_t get_fat_entry(FAT32_FS *fs, uint32_t cluster)
{
	if (cluster >= fs->total_fat_entries)
		return 0x0FFFFFFF; // end

	if (fs->fat_cache)
	{
		return fs->fat_cache[cluster] & 0x0FFFFFFF;
	}

	uint32_t fat_offset = cluster * 4;
	uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
	uint32_t offset = fat_offset % fs->bytes_per_sector;

	uint8_t *sector = kmalloc(fs->bytes_per_sector, GFP_KERNEL);
	if (!sector)
		return 0x0FFFFFFF;

	fs->read_sector(fs->device, fat_sector, sector);

	uint32_t entry = sector[offset] |
			 (sector[offset + 1] << 8) |
			 (sector[offset + 2] << 16) |
			 (sector[offset + 3] << 24);

	kfree(sector);

	return entry & 0x0FFFFFFF;
}

void set_fat_entry(FAT32_FS *fs, uint32_t cluster, uint32_t value)
{
	value &= 0x0FFFFFFF;
	if (fs->fat_cache && cluster < fs->total_fat_entries)
	{
		printk("Setting FAT entry cache %u to %u\n", cluster, value);
		fs->fat_cache[cluster] = value;
		fs->fat_dirty = true;
		return;
	}
	printk("Setting FAT entry %u to %u\n", cluster, value);
	//  fallback: sector read/modify/write
	uint32_t fat_offset = cluster * 4;
	uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
	uint8_t sector[512];
	// ata_read_sector(fs, fat_sector, sector);
	fat32_read_cluster(fs, cluster, sector);
	uint32_t offset = fat_offset % fs->bytes_per_sector;
	*((uint32_t *)(sector + offset)) = value;
	// ata_write_sector(fs, fat_sector, sector);
	fs->write_sector(fs->device, fat_sector, sector);
}

void fat32_free_cluster(FAT32_FS *fs, uint32_t cluster)
{
	if (cluster < 2 || cluster >= fs->total_fat_entries)
	{
		printk("Invalid cluster number: %u\n", cluster);
		return;
	}
	set_fat_entry(fs, cluster, 0x00000000);
	printk("Cluster %u freed\n", cluster);
}

uint32_t fat32_allocate_cluster(FAT32_FS *fs)
{
	if (!(fs->fat_cache))
	{
		// fallback: sector scan (slow)
		for (uint32_t i = 2; i < fs->total_fat_entries; ++i)
		{
			if (i == fs->root_cluster) // root
				continue;
			printk("Checking FAT entry %d\n", i);
			if (get_fat_entry(fs, i) == 0x00000000)
			{
				printk("Found free FAT entry %d\n", i);
				set_fat_entry(fs, i, 0x0FFFFFFF);
				printk("Allocated FAT entry %d\n", i);
				return i;
			}
		}
		return 0;
	}

	for (uint32_t i = 3; i < fs->total_fat_entries; ++i)
	{
		if ((fs->fat_cache[i] & 0x0FFFFFFF) == 0x00000000)
		{
			fs->fat_cache[i] = 0x0FFFFFFF;
			fs->fat_dirty = true;
			return i;
		}
	}
	return 0;
}
