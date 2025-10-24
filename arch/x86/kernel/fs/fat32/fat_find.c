#include "fat_utils.h"
#include "fat_structs.h"
#include "fat.h"

#include <stdio.h>
#include <string.h>

typedef void (*directory_entry_callback_t)(const char *name, bool is_dir, Directory *context);

uint32_t resolve_path_to_cluster(FAT32_FS *fs, const char *path)
{
	PathParts parts = format_folder_path(path);
	int depth = parts.count;
	printf("target %s depth %d", parts.parts[depth].sfn, depth);
	uint32_t cluster = fs->root_cluster;
	for (int i = 0; i < depth - 1; ++i)
	{
		printf("part: %s\n", parts.parts[i].sfn);
		cluster = find_directory_entry_cluster(fs, cluster, parts.parts[i].sfn);
		printf("cluster: %d\n", cluster);
		if (cluster == 0 || cluster >= 0x0FFFFFF8)
			return 0; // cluster not found
	}
	printf("cluster: %d\n", cluster);
	free_folder_path(&parts);
	return cluster;
}

uint32_t find_directory_entry_cluster(FAT32_FS *fs, uint32_t dir_cluster, const char *name11)
{
	printf("find_directory_entry_cluster: %s\n", name11);
	uint8_t *buffer = malloc(fs->cluster_size);
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
				free(buffer);
				printf("entry cluster: high = %d, low = %d\n", entry->first_cluster_high, entry->first_cluster_low);

				return (entry->first_cluster_high << 16) | entry->first_cluster_low;
			}
		}

		dir_cluster = get_fat_entry(fs, dir_cluster);
	}
	free(buffer);
	return 0;
}

void iterate_directory(FAT32_FS *fs, uint32_t cluster, directory_entry_callback_t callback, void *ctx)
{
	int steps = 0;
	uint8_t *data = malloc(fs->cluster_size);
	if (!data)
		return;
	while (cluster < 0x0FFFFFF8 && steps++ < MAX_CLUSTER_CHAIN && cluster != 0)
	{
		// printf("cluster: %d\n", cluster);
		fat32_read_cluster(fs, cluster, data);
		// printf("cluster: %d\n", cluster);
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
	free(data);
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
