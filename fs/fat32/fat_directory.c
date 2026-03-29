#include <hubble/string.h>

#include <hubble/printk.h>
#include "fat.h"
#include "fat_structs.h"
#include "fat_utils.h"
#include <mm/kmalloc.h>

void fat32_format_directory_cluster(FAT32_FS *fs, uint32_t cluster, uint32_t parent_cluster)
{
	uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
	memset(buf, 0, fs->cluster_size);

	// Entry "."
	FAT32_DirectoryEntry *dot = (FAT32_DirectoryEntry *)buf;
	memcpy(dot->name, ".          ", 11);
	dot->attr = 0x10;
	dot->first_cluster_high = (cluster >> 16) & 0xFFFF;
	dot->first_cluster_low = cluster & 0xFFFF;

	// Entry ".."
	FAT32_DirectoryEntry *dotdot = (FAT32_DirectoryEntry *)(buf + sizeof(FAT32_DirectoryEntry));
	memcpy(dotdot->name, "..         ", 11);
	dotdot->attr = 0x10;
	dotdot->first_cluster_high = (parent_cluster >> 16) & 0xFFFF;
	dotdot->first_cluster_low = parent_cluster & 0xFFFF;

	fat32_write_cluster(fs, cluster, buf);
	kfree(buf);
}

bool fat32_create_directory(FAT32_FS *fs, const char *path)
{
	PathParts parts = format_folder_path(path);
	printk("path: %s\n", path);
	printk("parts: %d\n", parts.count);

	const char abs_path[256] = {0};
	for (int i = 0; i < parts.count - 1; ++i)
	{
		if (i > 0)
			strcat(abs_path, "/");
		strcat(abs_path, parts.parts[i].sfn);
	}
	uint32_t dir_cluster = resolve_path_to_cluster(fs, abs_path);
	return fat32_create_entry(fs, dir_cluster, &parts.parts[parts.count - 1], true);
}

bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out, bool *is_dir_out)
{
	if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
		return false; // deleted

	if ((entry->attr & 0x0F) == 0x0F)
		return false; // LFN entry

	int pos = 0;
	for (int i = 0; i < 8 && entry->name[i] != ' '; ++i)
		name_out[pos++] = entry->name[i];

	if (entry->name[8] != ' ')
	{
		name_out[pos++] = '.';
		for (int i = 8; i < 11 && entry->name[i] != ' '; ++i)
			name_out[pos++] = entry->name[i];
	}

	name_out[pos] = '\0';
	*is_dir_out = (entry->attr & 0x10) != 0;
	return true;
}

bool fat32_add_directory_entry(FAT32_FS *fs, uint32_t dir_cluster, FAT32_DirectoryEntry *new_entry)
{
	uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);

	while (dir_cluster < 0x0FFFFFF8)
	{
		fat32_read_cluster(fs, dir_cluster, buf);
		size_t entries = fs->cluster_size / sizeof(FAT32_DirectoryEntry);

		for (size_t i = 0; i < entries; ++i)
		{
			FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
			if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
			{
				memcpy(entry, new_entry, sizeof(FAT32_DirectoryEntry));
				fat32_write_cluster(fs, dir_cluster, buf);
				kfree(buf);
				return true;
			}
		}

		dir_cluster = get_fat_entry(fs, dir_cluster);
	}

	kfree(buf);
	return false;
}
bool fat32_delete_directory(FAT32_FS *fs, const char *path)
{

	PathParts parts = format_folder_path(path);

	char abs_path[256] = {0};
	for (int i = 0; i < parts.count - 1; ++i)
	{
		if (i > 0)
			strcat(abs_path, "/");
		strcat(abs_path, parts.parts[i].sfn);
	}
	uint32_t parent_cluster = resolve_path_to_cluster(fs, abs_path);
	uint32_t dir_cluster = find_directory_entry_cluster(fs, parent_cluster, parts.parts[parts.count - 1].sfn);
	Directory dir = fat32_list_files(fs, dir_cluster);
	printk("dir count: %d\n", dir.count);

	if (dir.count > 2)
	{
		for (int i = 0; i < dir.count; i++)
		{
			printk("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
		}
		printk("Directory not empty\n");
		return false;
	}

	fat32_delete_entry(fs, parent_cluster, parts.parts[parts.count - 1].sfn);
	printk("📁 Directory deleted: %s\n", path);

	fat_flush(fs);
	return true;
}
