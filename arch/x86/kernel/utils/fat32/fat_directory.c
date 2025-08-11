#include "utils/fat32/fat.h"
#include "utils/fat32/fat_structs.h"
#include "utils/fat32/fat_utils.h"
#include <string.h>
#include <stdio.h>

void fat32_format_directory_cluster(uint32_t cluster, uint32_t parent_cluster)
{
    uint8_t *buf = malloc(cluster_size);
    memset(buf, 0, cluster_size);

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

    fat32_write_cluster(cluster, buf);
    free(buf);
}

bool fat32_create_directory(const char *path, const char *dirname11)
{
    char dirnametemp11[11];
    format_filename_fat(dirname11, dirnametemp11);

    uint32_t parent_cluster = resolve_path_to_cluster(path);
    if (parent_cluster == 0)
    {
        printf("Parent path not found: %s\n", path);
        return false;
    }
    // check if dir already exists
    uint32_t dir_cluster = find_directory_entry_cluster(parent_cluster, dirnametemp11);
    if (dir_cluster != 0)
    {
        printf("Directory already exists: %s\n", dirname11);
        return false;
    }

    // Allocate new cluster
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0 || new_cluster >= total_fat_entries || new_cluster == 2)
    {
        printf("No free clusters\n");
        return false;
    }

    // Initialize new directory
    fat32_format_directory_cluster(new_cluster, parent_cluster);

    // Create directory entry
    FAT32_DirectoryEntry entry = {0};
    memcpy(entry.name, dirnametemp11, 11);
    for (int i = 0; i < 11; i++)
    {
        printf("%c %c\n", entry.name[i], dirnametemp11[i]);
    }
    entry.attr = 0x10;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.file_size = 0;

    bool result = fat32_add_directory_entry(parent_cluster, &entry);
    if (!result)
    {
        printf(" Failed to add directory entry\n");
        fat32_free_cluster(new_cluster);
        return false;
    }

    printf("📁 Directory created: %s in %s (cluster %d)\n", dirnametemp11, path, new_cluster);
    fat_flush();
    return true;
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

bool fat32_add_directory_entry(uint32_t dir_cluster, FAT32_DirectoryEntry *new_entry)
{
    uint8_t *buf = malloc(cluster_size);

    while (dir_cluster < 0x0FFFFFF8)
    {
        fat32_read_cluster(dir_cluster, buf);
        size_t entries = cluster_size / sizeof(FAT32_DirectoryEntry);

        for (size_t i = 0; i < entries; ++i)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
            {
                memcpy(entry, new_entry, sizeof(FAT32_DirectoryEntry));
                fat32_write_cluster(dir_cluster, buf);
                free(buf);
                return true;
            }
        }

        dir_cluster = get_fat_entry(dir_cluster);
    }

    free(buf);
    return false;
}