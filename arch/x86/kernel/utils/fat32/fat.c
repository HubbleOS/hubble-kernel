#include "utils/fat32/fat.h"
#include "utils/fat32/fat_structs.h"
#include "utils/fat32/fat_utils.h"
#include "utils/ata/ata.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

uint8_t *fat_cache = NULL;
bool fat_dirty = false;

uint32_t fat_start_lba = 0;
uint32_t cluster_heap_lba = 0;
uint32_t root_cluster = 0;
uint32_t cluster_size = 0;
FAT32_BPB *bpb = NULL;
uint16_t total_fat_entries = 0;

typedef struct
{
    char *buffer;
    size_t pos;
} list_ctx_t;

void list_files_callback(const char *name, bool is_dir, void *ctx_ptr)
{
    list_ctx_t *ctx = (list_ctx_t *)ctx_ptr;
    // without sprintf
    ctx->buffer[ctx->pos++] = is_dir ? '/' : ' ';
    memcpy(ctx->buffer + ctx->pos, name, strlen(name));
    ctx->pos += strlen(name);
    ctx->buffer[ctx->pos++] = '\n';
}

void fat32_list_files(uint32_t cluster, char *out_buf)
{
    memset(out_buf, 0, 2048);
    list_ctx_t ctx = {.buffer = out_buf, .pos = 0};
    iterate_directory(cluster, list_files_callback, &ctx);
    out_buf[ctx.pos] = '\0';
}
void fat32_list_files_from_path(const char *path, char *out_buf)
{
    memset(out_buf, 0, 2048);
    uint32_t cluster = resolve_path_to_cluster(path);
    if (cluster == 0)
    {
        return;
    }

    fat32_list_files(cluster, out_buf);
}

uint32_t get_fat_entry(uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / bpb->bytes_per_sector);

    uint8_t sector[512];
    ata_read_sector(fat_sector, sector);

    uint32_t offset = fat_offset % bpb->bytes_per_sector;
    return *((uint32_t *)(sector + offset)) & 0x0FFFFFFF;
}
void set_fat_entry(uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / bpb->bytes_per_sector);

    uint8_t sector[512];
    ata_read_sector(fat_sector, sector);

    uint32_t offset = fat_offset % bpb->bytes_per_sector;
    *((uint32_t *)(sector + offset)) = value;
    ata_write_sector(fat_sector, sector);
}

void fat32_free_cluster(uint32_t cluster)
{
    if (cluster < 2 || cluster >= total_fat_entries)
    {
        printf("Invalid cluster number: %u\n", cluster);
        return;
    }

    // Mark the cluster as free (0x00000000)
    fat_cache[cluster] = 0x00000000;
    fat_dirty = true; // Mark the FAT as dirty
    printf(" Cluster %u marked as free\n", cluster);
}

uint32_t fat32_allocate_cluster()
{
    for (uint32_t i = 3; i < total_fat_entries; ++i)
    {
        if (get_fat_entry(i) == 0x00000000) // 0 = free
        {
            set_fat_entry(i, 0x0FFFFFFF); // Mark as end-of-chain
            return i;
        }
    }

    return 0; // No free cluster found
}

// int fat32_read_file(uint32_t start_cluster, uint8_t *buffer, uint32_t size)
// {
//     uint32_t cluster = start_cluster;
//     uint32_t bytes_per_cluster = cluster_size;
//     uint32_t bytes_read = 0;

//     while (cluster < 0x0FFFFFF8 && bytes_read < size)
//     {
//         uint8_t *cluster_ptr = get_cluster_ptr(cluster);
//         uint32_t to_copy = (size - bytes_read < bytes_per_cluster)
//                                ? (size - bytes_read)
//                                : bytes_per_cluster;

//         memcpy(buffer + bytes_read, cluster_ptr, to_copy);
//         bytes_read += to_copy;

//         cluster = get_next_cluster(cluster); // ⚠️ використовує кеш
//     }

//     return bytes_read;
// }

// // int fat32_read_file(const char *path, void *out_buf, size_t *out_size)
// // {
// //     char **folder_path = format_folder_path(path);
// //     int depth = atoi(folder_path[0]);
// //     uint32_t dir_cluster = 0;
// //     if (depth <= 1)
// //         dir_cluster = root_cluster;
// //     else
// //         dir_cluster = fat32_find_dir(folder_path);
// //     if (dir_cluster < 2)
// //         return -1;

// //     char *target_name = folder_path[depth];
// //     FAT32_DirectoryEntry *entry = fat32_find_file_in(dir_cluster, target_name);

// //     if (!entry)
// //         return -1;

// //     uint32_t file_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
// //     uint32_t bytes_read = 0;
// //     uint8_t *out = (uint8_t *)out_buf;

// //     while (file_cluster < 0x0FFFFFF8)
// //     {
// //         uint8_t *src = get_cluster_ptr(file_cluster);
// //         if (!src)
// //             return -1;

// //         uint32_t to_copy = cluster_size;
// //         if (bytes_read + to_copy > entry->file_size)
// //             to_copy = entry->file_size - bytes_read;

// //         memcpy(out + bytes_read, src, to_copy);
// //         bytes_read += to_copy;

// //         if (bytes_read >= entry->file_size)
// //             break;

// //         file_cluster = get_next_cluster(file_cluster);
// //     }

// //     if (out_size)
// //         *out_size = bytes_read;
// //     return 0; // успішне прочитання
// // }

// int fat32_write_file(const char *path, const void *data, size_t size)
// {
//     char **folder_path = format_folder_path(path);
//     int depth = atoi(folder_path[0]);
//     uint32_t dir_cluster = 0;
//     if (depth <= 1)
//         dir_cluster = root_cluster;
//     else
//         dir_cluster = fat32_find_dir(folder_path);
//     if (dir_cluster < 2)
//         return -1;

//     char *target = folder_path[depth];

//     uint32_t cluster = dir_cluster;
//     int cluster_steps = 0;

//     FAT32_DirectoryEntry *existing_entry = NULL;
//     uint8_t *cluster_ptr = NULL;

//     // Шукаємо існуючий файл або вільний запис у директорії
//     while (cluster < 0x0FFFFFF8 && cluster_steps++ < MAX_CLUSTER_CHAIN)
//     {
//         cluster_ptr = get_cluster_ptr(cluster);
//         if (!cluster_ptr)
//             return -2;

//         for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
//         {
//             FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));

//             if (memcmp(entry->name, target, 11) == 0)
//             {
//                 existing_entry = entry;
//                 goto found_entry;
//             }
//             if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
//             {
//                 if (!existing_entry)
//                     existing_entry = entry; // вільний запис
//             }
//         }

//         cluster = get_next_cluster(cluster);
//     }

// found_entry:

//     if (!existing_entry)
//         return -3; // Нема вільного запису і немає існуючого файлу

//     // Якщо файл існує, звільняємо кластери у FAT
//     if (existing_entry->name[0] != 0x00 && existing_entry->name[0] != 0xE5)
//     {
//         uint32_t file_cluster = (existing_entry->first_cluster_high << 16) | existing_entry->first_cluster_low;
//         uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);

//         while (file_cluster < 0x0FFFFFF8 && file_cluster >= 2)
//         {
//             uint32_t next = fat[file_cluster] & 0x0FFFFFFF;
//             fat[file_cluster] = 0; // звільнити кластер
//             if (next >= 0x0FFFFFF8)
//                 break;
//             file_cluster = next;
//         }
//     }

//     // Знайти вільні кластери для нового файлу
//     uint32_t needed_clusters = (size + cluster_size - 1) / cluster_size;
//     uint32_t first_cluster = 0, prev_cluster = 0;
//     uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
//     uint32_t fat_entries = bpb->fat_size_32 * bpb->bytes_per_sector / 4;
//     uint32_t found = 0;

//     for (uint32_t i = 2; i < fat_entries && found < needed_clusters; i++)
//     {
//         if ((fat[i] & 0x0FFFFFFF) == 0)
//         {
//             fat[i] = 0x0FFFFFFF; // кінець ланцюга поки що
//             if (prev_cluster != 0)
//                 fat[prev_cluster] = i;
//             else
//                 first_cluster = i;
//             prev_cluster = i;
//             found++;
//         }
//     }

//     if (found < needed_clusters)
//         return -4; // Недостатньо місця

//     // Записуємо дані у кластери
//     const uint8_t *src = (const uint8_t *)data;
//     uint32_t bytes_written = 0;
//     uint32_t current_cluster = first_cluster;

//     while (bytes_written < size && current_cluster < 0x0FFFFFF8)
//     {
//         uint8_t *dest = get_cluster_ptr(current_cluster);
//         if (!dest)
//             return -5;

//         uint32_t to_copy = cluster_size;
//         if (bytes_written + to_copy > size)
//             to_copy = size - bytes_written;

//         memcpy(dest, src + bytes_written, to_copy);
//         bytes_written += to_copy;

//         current_cluster = get_next_cluster(current_cluster);
//     }

//     // Оновлюємо директорний запис
//     memcpy(existing_entry->name, target, 11);
//     existing_entry->attr = 0x20; // файл
//     existing_entry->first_cluster_low = first_cluster & 0xFFFF;
//     existing_entry->first_cluster_high = (first_cluster >> 16) & 0xFFFF;
//     existing_entry->file_size = size;

//     return 0;
// }

// int fat32_delete_file(const char *filename)
// {
//     char **folder_path = format_folder_path(filename);
//     int depth = atoi(folder_path[0]);
//     uint32_t dir_cluster = 0;
//     if (depth <= 1)
//         dir_cluster = root_cluster;
//     else
//         dir_cluster = fat32_find_dir(folder_path);
//     if (dir_cluster < 2)
//         return -1;

//     char *target = folder_path[depth];

//     FAT32_DirectoryEntry *entry = fat32_find_file_in(dir_cluster, target);
//     if (!entry)
//         return -1;

//     // Позначаємо файл як видалений
//     entry->name[0] = 0xE5;

//     // Очищення кластерів
//     uint32_t file_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
//     uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);

//     while (file_cluster < 0x0FFFFFF8)
//     {
//         uint32_t next = fat[file_cluster] & 0x0FFFFFFF;
//         fat[file_cluster] = 0x00000000;
//         file_cluster = next;
//     }

//     return 0;
// }

// int fat32_rename_file(const char *oldname, const char *newname)
// {
//     char **folder_path = format_folder_path(oldname);
//     char *folder_path_new;
//     format_filename_fat(newname, folder_path_new);
//     int depth = atoi(folder_path[0]);
//     uint32_t dir_cluster = 0;
//     if (depth <= 1)
//         dir_cluster = root_cluster;
//     else
//         dir_cluster = fat32_find_dir(folder_path);
//     if (dir_cluster < 2)
//         return -1;

//     char *target = folder_path[depth];
//     if (folder_path_new == folder_path[depth])
//     {
//         return 0;
//     }

//     FAT32_DirectoryEntry *entry = fat32_find_file_in(dir_cluster, target);
//     if (!entry)
//         return -2;
//     memcpy(entry->name, folder_path_new, 11);
//     return 0;
// }
// int fat32_list_files(const char *path, char *out_buf)
// {
//     char **folder_path = format_folder_path(path);
//     int depth = atoi(folder_path[0]);
//     uint32_t dir_cluster = root_cluster;
//     if (depth >= 1)
//     {
//         dir_cluster = fat32_find_dir(folder_path);
//         if (dir_cluster < 2)
//             return -1;
//     }

//     uint32_t cluster = dir_cluster;
//     int cluster_steps = 0;
//     char *ptr = out_buf;

//     while (cluster < 0x0FFFFFF8 && cluster_steps++ < MAX_CLUSTER_CHAIN)
//     {
//         uint8_t *cluster_ptr = get_cluster_ptr(cluster);
//         if (!cluster_ptr)
//             return 0;

//         for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
//         {
//             FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));

//             if (entry->name[0] == 0x00)
//                 return 0;

//             if (entry->name[0] == 0xE5)
//                 continue;

//             if ((entry->attr & 0x0F) == 0x0F)
//                 continue;

//             // Скопіювати імʼя
//             for (int j = 0; j < 11; j++)
//             {
//                 char c = entry->name[j];
//                 if (c == ' ')
//                     break;
//                 *ptr++ = c;
//             }

//             // Додати мітку: [D] - directory, [F] - file
//             *ptr++ = ' ';
//             *ptr++ = '[';
//             *ptr++ = (entry->attr & 0x10) ? 'D' : 'F';
//             *ptr++ = ']';
//             *ptr++ = '\n';
//         }

//         cluster = get_next_cluster(cluster);
//     }

//     *ptr = '\0';
//     return 0;
// }
// int fat32_create_folder(const char *path)
// {
//     char **folder_path = format_folder_path(path);
//     int depth = atoi(folder_path[0]);
//     uint32_t dir_cluster = root_cluster;
//     if (depth - 1 >= 1)
//     {
//         dir_cluster = fat32_find_dir(folder_path);
//         if (dir_cluster < 2)
//             return -1;
//     }

//     // 1. Пошук вільного запису у директорії
//     uint32_t cluster = dir_cluster;
//     FAT32_DirectoryEntry *free_entry = NULL;
//     uint8_t *parent_buf = NULL;

//     while (cluster < 0x0FFFFFF8)
//     {
//         parent_buf = get_cluster_ptr(cluster);
//         for (int i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
//         {
//             FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(parent_buf + i * sizeof(FAT32_DirectoryEntry));
//             if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
//             {
//                 free_entry = entry;
//                 goto found_free;
//             }
//         }
//         cluster = get_next_cluster(cluster);
//     }
//     return -2;

// found_free:
//     // 2. Виділяємо новий кластер
//     uint32_t new_cluster = 0;
//     uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
//     for (uint32_t i = 2; i < bpb->fat_size_32 * bpb->bytes_per_sector / 4; i++)
//     {
//         if ((fat[i] & 0x0FFFFFFF) == 0)
//         {
//             fat[i] = 0x0FFFFFFF;
//             new_cluster = i;
//             break;
//         }
//     }
//     if (new_cluster == 0)
//         return -3;

//     // 3. Створення запису про нову папку
//     memcpy(free_entry->name, folder_path[depth], 11);
//     free_entry->attr = 0x10; // directory
//     free_entry->first_cluster_low = new_cluster & 0xFFFF;
//     free_entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
//     free_entry->file_size = 0;

//     // 4. Очищення нового кластеру
//     uint8_t *new_buf = get_cluster_ptr(new_cluster);
//     memset(new_buf, 0, cluster_size);

//     // 5. Створюємо `.` та `..`
//     FAT32_DirectoryEntry *entries = (FAT32_DirectoryEntry *)new_buf;

//     // `.`
//     memset(&entries[0], 0, sizeof(FAT32_DirectoryEntry));
//     memcpy(entries[0].name, ".          ", 11);
//     entries[0].attr = 0x10;
//     entries[0].first_cluster_low = new_cluster & 0xFFFF;
//     entries[0].first_cluster_high = (new_cluster >> 16) & 0xFFFF;

//     // `..`
//     memset(&entries[1], 0, sizeof(FAT32_DirectoryEntry));
//     memcpy(entries[1].name, "..         ", 11);
//     entries[1].attr = 0x10;
//     entries[1].first_cluster_low = dir_cluster & 0xFFFF;
//     entries[1].first_cluster_high = (dir_cluster >> 16) & 0xFFFF;

//     // 6. Записати FAT на диск
//     for (int i = 0; i < bpb->num_fats; i++)
//     {
//         uint32_t fat_lba = fat_start_lba + i * bpb->fat_size_32;
//         for (uint32_t s = 0; s < bpb->fat_size_32; s++)
//         {
//             ata_write_sector(fat_lba + s,
//                              (uint8_t *)fat + s * bpb->bytes_per_sector);
//         }
//     }

//     // 7. Записати батьківську директорію (важливо: `parent_buf`)
//     ata_write_cluster(dir_cluster, parent_buf);

//     // 8. Записати нову директорію
//     ata_write_cluster(new_cluster, new_buf);

//     return 0;
// }

// int fat32_delete_dir(const char *path)
// {
//     char **folder_path = format_folder_path(path);
//     int depth = atoi(folder_path[0]);
//     uint32_t parent_cluster = root_cluster;
//     if (depth - 1 >= 1)
//     {
//         parent_cluster = fat32_find_dir(folder_path);
//         if (parent_cluster < 2)
//             return -1;
//     }

//     // get target
//     char *target = folder_path[depth];

//     // find dir
//     uint32_t cluster = parent_cluster;
//     FAT32_DirectoryEntry *dir_entry = NULL;

//     while (cluster < 0x0FFFFFF8)
//     {
//         uint8_t *cluster_ptr = get_cluster_ptr(cluster);
//         for (int i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
//         {
//             FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));
//             if (entry->name[0] == 0x00)
//                 break;

//             if ((entry->attr & 0x0F) == 0x0F)
//                 continue;

//             if ((entry->attr & 0x10) && memcmp(entry->name, target, 11) == 0)
//             {
//                 dir_entry = entry;
//                 goto found_dir;
//             }
//         }
//         cluster = get_next_cluster(cluster);
//     }
//     return -3; // not found

// found_dir:
//     // check if dir is empty
//     uint32_t dir_cluster = (dir_entry->first_cluster_high << 16) | dir_entry->first_cluster_low;
//     uint8_t *cluster_ptr = get_cluster_ptr(dir_cluster);
//     FAT32_DirectoryEntry *entries = (FAT32_DirectoryEntry *)cluster_ptr;

//     for (int i = 2; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
//     {
//         if (entries[i].name[0] == 0x00)
//             break; // end
//         if (entries[i].name[0] != 0xE5)
//             return -4; // dir is not empty
//     }

//     // Очистити FAT
//     uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
//     uint32_t next = fat[dir_cluster] & 0x0FFFFFFF;
//     fat[dir_cluster] = 0;
//     while (next < 0x0FFFFFF8)
//     {
//         uint32_t temp = fat[next] & 0x0FFFFFFF;
//         fat[next] = 0;
//         next = temp;
//     }

//     // Видалити запис у батьківській директорії
//     dir_entry->name[0] = 0xE5;

//     return 0;
// }