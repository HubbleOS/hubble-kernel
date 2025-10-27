#include "fat.h"
#include "printk.h"
#include "fat_structs.h"
#include "fat_utils.h"
#include <fs/ata/ata.h>
#include <mm/kmalloc.h>

#include <string.h>

#include <stdbool.h>

bool fat32_create_file(FAT32_FS *fs, const char *path)
{
	printk("Creating file: %s\n", path);
	PathParts parts = format_folder_path(path);
	char abs_path[256] = {0};
	abs_path[0] = '\0';
	printk("parts: %d\n", parts.count);
	printk("parts: %s\n", parts.parts[parts.count - 1].sfn);
	for (int i = 0; i < parts.count - 1; ++i)
	{
		if (i > 0)
			strcat(abs_path, "/");
		strcat(abs_path, parts.parts[i].sfn);
	}

	uint32_t parent_cluster = resolve_path_to_cluster(fs, abs_path);
	printk("parent cluster: %d\n", parent_cluster);
	return fat32_create_entry(fs, parent_cluster, &parts.parts[parts.count - 1], false);
}

// bool fat32_write_file(FAT32_FS *fs, const char *path, const char *filename11, const uint8_t *data, size_t size)
// {

// 	char target[11];
// 	format_filename_fat(filename11, target);

// 	uint32_t file_cluster = resolve_path_to_cluster(fs, path);
// 	if (file_cluster == 0)
// 	{
// 		printk(" File not found: %s/%s\n", path, filename11);
// 		return false;
// 	}

// 	// Find entry
// 	uint8_t *buf = kmalloc(fs->cluster_size);
// 	if (!buf)
// 	{
// 		printk("Failed to init buf");
// 	}
// 	fat32_read_cluster(fs, file_cluster, buf);
// 	size_t entries = fs->cluster_size / sizeof(FAT32_DirectoryEntry);
// 	FAT32_DirectoryEntry *entry = NULL;

// 	for (size_t i = 0; i < entries; ++i)
// 	{
// 		FAT32_DirectoryEntry *e = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
// 		if (memcmp(e->name, target, 11) == 0 && !(e->attr & 0x10))
// 		{
// 			printk("found entry to write\n");

// 			entry = e;
// 			break;
// 		}
// 	}

// 	if (!entry)
// 	{
// 		free(buf);
// 		printk("Entry not found in cluster\n");
// 		return false;
// 	}

// 	// Запис у кластери
// 	uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
// 	size_t remaining = size;
// 	size_t offset = 0;

// 	while (remaining > 0)
// 	{
// 		uint8_t *write_buf = kmalloc(fs->cluster_size);
// 		size_t to_write = remaining > fs->cluster_size ? fs->cluster_size : remaining;
// 		memcpy(write_buf, data + offset, to_write);
// 		fat32_write_cluster(fs, cluster, write_buf);
// 		free(write_buf);

// 		remaining -= to_write;
// 		offset += to_write;

// 		if (remaining > 0)
// 		{
// 			uint32_t next = get_fat_entry(fs, cluster);
// 			if (next >= 0x0FFFFFF8)
// 			{
// 				next = fat32_allocate_cluster(fs);
// 				if (next == 0)
// 				{
// 					printk("No space during write\n");
// 					free(buf);
// 					return false;
// 				}
// 				set_fat_entry(fs, cluster, next);
// 			}
// 			cluster = next;
// 		}
// 	}
// 	entry->file_size = size;
// 	fat32_write_cluster(fs, file_cluster, buf);
// 	free(buf);
// 	return true;
// }

// size_t fat32_read_file(FAT32_FS *fs, const char *path, const char *filename11, uint8_t *out_buf, size_t max_size)
// {
// 	char target[11];
// 	format_filename_fat(filename11, target);
// 	uint32_t dir_cluster = resolve_path_to_cluster(fs, path);
// 	if (dir_cluster == 0)
// 	{
// 		printk("Path not found: %s\n", path);
// 		return 0;
// 	}

// 	uint8_t *buf = kmalloc(fs->cluster_size);
// 	fat32_read_cluster(fs, dir_cluster, buf);
// 	FAT32_DirectoryEntry *entry = NULL;

// 	for (size_t i = 0; i < fs->cluster_size / sizeof(FAT32_DirectoryEntry); ++i)
// 	{
// 		FAT32_DirectoryEntry *e = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
// 		if (memcmp(e->name, target, 11) == 0 && !(e->attr & 0x10))
// 		{
// 			printk("found entry to read\n");
// 			entry = e;
// 			break;
// 		}
// 	}

// 	if (!entry)
// 	{
// 		printk("File not found: %s/%s\n", path, filename11);
// 		free(buf);
// 		return 0;
// 	}

// 	uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
// 	size_t file_size = entry->file_size;
// 	size_t to_read = file_size < max_size ? file_size : max_size;

// 	size_t read = 0;
// 	while (read < to_read && cluster < 0x0FFFFFF8)
// 	{
// 		uint8_t *cluster_buf = kmalloc(fs->cluster_size);
// 		fat32_read_cluster(fs, cluster, cluster_buf);

// 		size_t chunk = (to_read - read) > fs->cluster_size ? fs->cluster_size : (to_read - read);
// 		memcpy(out_buf + read, cluster_buf, chunk);
// 		read += chunk;

// 		free(cluster_buf);
// 		cluster = get_fat_entry(fs, cluster);
// 	}

// 	free(buf);
// 	return read;
// }
bool fat32_delete_file(FAT32_FS *fs, const char *path)
{
	PathParts pp = format_folder_path(path);
	char abs_path[256] = {0};
	for (int i = 0; i < pp.count - 1; ++i)
	{
		if (i > 0)
			strcat(abs_path, "/");
		strcat(abs_path, pp.parts[i].sfn);
	}
	uint32_t parent_cluster = resolve_path_to_cluster(fs, abs_path);

	fat32_delete_entry(fs, parent_cluster, pp.parts[pp.count - 1].sfn);
	printk("File deleted: %s/%s\n", path, pp.parts[pp.count - 1].sfn);
	return true;
}
