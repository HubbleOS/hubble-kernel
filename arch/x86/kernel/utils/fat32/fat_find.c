#include "utils/fat32/fat_utils.h"
#include "utils/fat32/fat_structs.h"

#include <stdio.h>
#include <string.h>

typedef void (*directory_entry_callback_t)(const char *name, bool is_dir, void *context);

uint32_t resolve_path_to_cluster(const char *path)
{
    char **parts = format_folder_path(path);
    int depth = atoi(parts[0]);
    printf("target %s", parts[depth]);
    uint32_t cluster = root_cluster;
    for (int i = depth; i <= depth; ++i)
    {

        cluster = find_directory_entry_cluster(cluster, parts[i]);
        printf("cluster: %d\n", cluster);
        if (cluster == 0 || cluster >= 0x0FFFFFF8)
            return 0; // cluster not found
    }
    return cluster;
}

uint32_t find_directory_entry_cluster(uint32_t dir_cluster, const char *name11)
{
    printf("find_directory_entry_cluster: %s\n", name11);
    uint8_t *buffer = malloc(cluster_size);
    while (dir_cluster < 0x0FFFFFF8)
    {
        fat32_read_cluster(dir_cluster, buffer);
        size_t entries = cluster_size / sizeof(FAT32_DirectoryEntry);

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

        dir_cluster = get_fat_entry(dir_cluster);
    }
    free(buffer);
    return 0;
}

void iterate_directory(uint32_t cluster, directory_entry_callback_t callback, void *ctx)
{
    int steps = 0;
    while (cluster < 0x0FFFFFF8 && steps++ < MAX_CLUSTER_CHAIN)
    {

        uint8_t *data = malloc(cluster_size);
        fat32_read_cluster(cluster, data);
        size_t entries = cluster_size / sizeof(FAT32_DirectoryEntry);
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

        cluster = get_fat_entry(cluster);
    }
}