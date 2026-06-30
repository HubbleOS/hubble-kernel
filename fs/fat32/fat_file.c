/* ── FAT32 file operations ────────────────────────────────────────
 * File creation and deletion functions for the FAT32 filesystem.
 * Uses path parsing, directory cluster resolution, and entry
 * manipulation from the utility layer.
 * ────────────────────────────────────────────────────────────────── */

#include "fat.h"
#include <hubble/printk.h>
#include "fat_structs.h"
#include "fat_utils.h"
#include <mm/kmalloc.h>

#include <hubble/string.h>

#include <stdbool.h>

/** @brief Create a new file at the given path. */
bool fat32_create_file(FAT32_FS *fs, const char *path)
{
	printk(KERN_INFO "Creating file: %s\n", path);
	PathParts parts = format_folder_path(path);
	char abs_path[256] = {0};
	abs_path[0] = '\0';
	printk(KERN_INFO "parts: %d\n", parts.count);
	printk(KERN_INFO "parts: %s\n", parts.parts[parts.count - 1].sfn);
	for (int i = 0; i < parts.count - 1; ++i)
	{
		if (i > 0)
			strcat(abs_path, "/");
		strcat(abs_path, parts.parts[i].sfn);
	}

	uint32_t parent_cluster = resolve_path_to_cluster(fs, abs_path);
	printk(KERN_INFO "parent cluster: %d\n", parent_cluster);
	return fat32_create_entry(fs, parent_cluster, &parts.parts[parts.count - 1], false);
}

/** @brief Delete a file at the given path. */
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
	printk(KERN_INFO "File deleted: %s/%s\n", path, pp.parts[pp.count - 1].sfn);
	return true;
}
