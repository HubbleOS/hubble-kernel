#include "fat_utils.h"
#include "fat_structs.h"
#include "fat.h"
#include <hubble/printk.h>

#include <mm/kmalloc.h>

#include <hubble/string.h>

typedef void (*directory_entry_callback_t)(const char *name, bool is_dir, Directory *context);

uint32_t resolve_path_to_cluster(FAT32_FS *fs, const char *path)
{
	printk("Resolving path to cluster: %s\n", path);
	PathParts parts = format_folder_path(path);
	int depth = parts.count;
	printk("target %s depth %d", parts.parts[depth].sfn, depth);

	uint32_t cluster = fs->root_cluster;
	for (int i = 0; i < depth - 1; ++i)
	{
		printk("part: %s\n", parts.parts[i].sfn);
		cluster = find_directory_entry_cluster(fs, cluster, parts.parts[i].sfn);
		printk("cluster: %d\n", cluster);
		if (cluster == 0 || cluster >= 0x0FFFFFF8)
			return 0; // cluster not found
	}

	printk("cluster: %d\n", cluster);
	free_folder_path(&parts);
	return cluster;
}

uint32_t find_directory_entry_cluster(FAT32_FS *fs, uint32_t dir_cluster, const char *name11)
{
	if (!fs || !name11)
	{
		if (!fs)
		{
			printk("find_directory_entry_cluster: fs is null\n");
		}
		else
		{
			printk("find_directory_entry_cluster: name11 is null\n");
		}
		printk("find_directory_entry_cluster: invalid parameters \n");
		return 0;
	}

	printk("find_directory_entry_cluster: %s\n", name11);

	if (dir_cluster == 0)
		dir_cluster = 2;

	uint8_t *buffer = kmalloc(fs->cluster_size, GFP_KERNEL);
	if (!buffer)
	{
		printk("find_directory_entry_cluster: failed to allocate buffer\n");
		return 0;
	}

	int steps = 0;
	while (dir_cluster < 0x0FFFFFF8 && steps++ < MAX_CLUSTER_CHAIN)
	{
		fat32_read_cluster(fs, dir_cluster, buffer);
		size_t entries = fs->cluster_size / sizeof(FAT32_DirectoryEntry);

		for (size_t i = 0; i < entries; ++i)
		{
			FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(buffer + i * sizeof(FAT32_DirectoryEntry));

			if ((entry->attr & 0x0F) == 0x0F || entry->name[0] == 0x00 || entry->name[0] == 0xE5)
				continue;

			if (memcmp(entry->name, name11, 11) == 0)
			{
				uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
				printk("found entry cluster: high=%04x low=%04x (cluster=%08x)\n",
					   entry->first_cluster_high, entry->first_cluster_low, cluster);
				kfree(buffer);
				return cluster;
			}
		}

		dir_cluster = get_fat_entry(fs, dir_cluster);
	}

	kfree(buffer);
	return 0;
}

void iterate_directory(FAT32_FS *fs, uint32_t cluster, directory_entry_callback_t callback, void *ctx)
{
	int steps = 0;
	uint8_t *data = kmalloc(fs->cluster_size, GFP_KERNEL);
	if (!data)
		return;
	while (cluster < 0x0FFFFFF8 && steps++ < MAX_CLUSTER_CHAIN && cluster != 0)
	{
		printk("cluster: %d\n", cluster);
		fat32_read_cluster(fs, cluster, data);
		printk("cluster: %d\n", cluster);
		size_t entries = fs->cluster_size / sizeof(FAT32_DirectoryEntry);

		for (size_t i = 0; i < entries; ++i)
		{
			FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(data + i * sizeof(FAT32_DirectoryEntry));
			char name[20];
			bool is_dir;
			if (parse_directory_entry(entry, name, &is_dir))
			{
				callback(name, is_dir, ctx);
			}
		}

		cluster = get_fat_entry(fs, cluster);
	}

	kfree(data);
}

// bool is_dir(FAT32_DirectoryEntry *entry)
// {
//     return (entry->attr & 0x10) == 0x10;
// }
// bool is_file(FAT32_DirectoryEntry *entry)
// {
//     return (entry->attr & 0x10) != 0x10;
// }
// bool is_empty_dir(FAT32_DirectoryEntry *entry)
// {
//     if (is_dir(entry))
//         return false;

//     Directory empty = fat32_list_files(fs, entry->first_cluster_high << 16 | entry->first_cluster_low);
//     if (empty.count == 0)
//         return true;
//     return false;
// }
