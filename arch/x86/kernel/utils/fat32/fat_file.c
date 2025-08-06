#include "utils/fat32/fat.h"
#include "utils/fat32/fat_structs.h"
#include "utils/fat32/fat_utils.h"
#include "utils/ata/ata.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

bool fat32_create_file(const char *path, const char *filename11)
{
    uint32_t parent_cluster = resolve_path_to_cluster(path);
    if (parent_cluster == 0)
    {
        printf("Parent path not found: %s\n", path);
        return false;
    }

    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0)
    {
        printf("No free clusters\n");
        return false;
    }
    char target[11];
    format_filename_fat(filename11, target);
    // Create the file
    FAT32_DirectoryEntry entry = {0};
    memcpy(entry.name, target, 11); // "NAME    EXT"
    entry.attr = 0x20;              // 0x20 = file
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.file_size = 0;

    if (!fat32_add_directory_entry(parent_cluster, &entry))
    {
        printf("Failed to add file entry\n");
        fat32_free_cluster(new_cluster);
        return false;
    }

    printf("File created: %s in %s (cluster %d)\n", filename11, path, new_cluster);
    return true;
}

bool fat32_write_file(const char *path, const char *filename11, const uint8_t *data, size_t size)
{

    char target[11];
    format_filename_fat(filename11, target);

    uint32_t file_cluster = resolve_path_to_cluster(path);
    if (file_cluster == 0)
    {
        printf(" File not found: %s/%s\n", path, filename11);
        return false;
    }

    // Find entry
    uint8_t *buf = malloc(cluster_size);
    fat32_read_cluster(file_cluster, buf);
    size_t entries = cluster_size / sizeof(FAT32_DirectoryEntry);
    FAT32_DirectoryEntry *entry = NULL;

    for (size_t i = 0; i < entries; ++i)
    {
        FAT32_DirectoryEntry *e = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
        if (memcmp(e->name, target, 11) == 0 && !(e->attr & 0x10))
        {
            entry = e;
            break;
        }
    }

    if (!entry)
    {
        free(buf);
        printf("Entry not found in cluster\n");
        return false;
    }

    // Запис у кластери
    uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    size_t remaining = size;
    size_t offset = 0;

    while (remaining > 0)
    {
        uint8_t *write_buf = malloc(cluster_size);
        size_t to_write = remaining > cluster_size ? cluster_size : remaining;
        memcpy(write_buf, data + offset, to_write);
        fat32_write_cluster(cluster, write_buf);
        free(write_buf);

        remaining -= to_write;
        offset += to_write;

        if (remaining > 0)
        {
            uint32_t next = get_fat_entry(cluster);
            if (next >= 0x0FFFFFF8)
            {
                next = fat32_allocate_cluster();
                if (next == 0)
                {
                    printf("No space during write\n");
                    free(buf);
                    return false;
                }
                set_fat_entry(cluster, next);
            }
            cluster = next;
        }
    }

    // Оновлюємо розмір
    entry->file_size = size;
    fat32_write_cluster(file_cluster, buf);
    free(buf);
    return true;
}

size_t fat32_read_file(const char *path, const char *filename11, uint8_t *out_buf, size_t max_size)
{
    char target[11];
    format_filename_fat(filename11, target);
    uint32_t dir_cluster = resolve_path_to_cluster(path);
    if (dir_cluster == 0)
    {
        printf("Path not found: %s\n", path);
        return 0;
    }

    uint8_t *buf = malloc(cluster_size);
    fat32_read_cluster(dir_cluster, buf);
    FAT32_DirectoryEntry *entry = NULL;

    for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); ++i)
    {
        FAT32_DirectoryEntry *e = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
        if (memcmp(e->name, target, 11) == 0 && !(e->attr & 0x10))
        {
            entry = e;
            break;
        }
    }

    if (!entry)
    {
        printf("File not found: %s/%s\n", path, filename11);
        free(buf);
        return 0;
    }

    uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    size_t file_size = entry->file_size;
    size_t to_read = file_size < max_size ? file_size : max_size;

    size_t read = 0;
    while (read < to_read && cluster < 0x0FFFFFF8)
    {
        uint8_t *cluster_buf = malloc(cluster_size);
        fat32_read_cluster(cluster, cluster_buf);

        size_t chunk = (to_read - read) > cluster_size ? cluster_size : (to_read - read);
        memcpy(out_buf + read, cluster_buf, chunk);
        read += chunk;

        free(cluster_buf);
        cluster = get_fat_entry(cluster);
    }

    free(buf);
    return read;
}
bool fat32_delete_file(const char *filename)
{
    return false;
}
