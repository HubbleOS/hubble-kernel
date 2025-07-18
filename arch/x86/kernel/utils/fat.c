#include "fat.h"
#include "utils/fat_structs.h"
#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#define MAX_CLUSTER_CHAIN 1024

static uint8_t *fat_cache = NULL;
static bool fat_dirty = false;

uint32_t fat_start_lba, cluster_heap_lba, root_cluster;
static uint32_t cluster_size;
static FAT32_BPB *bpb = NULL;
static uint16_t total_fat_entries = 0;

void itos(int num, char *str);
void stoi(char *str, int *num);
void uint_to_str(uint32_t num, char *buf, size_t bufsize);

void list_files_callback(const char *name, bool is_dir, void *ctx_ptr);
int fat32_init_from_lba(gpt_partition_t part);
uint32_t cluster_to_lba(uint32_t cluster);
void fat32_read_cluster(uint32_t cluster, uint8_t *buffer);
void ata_write_cluster(uint32_t cluster, const uint8_t *data);
uint32_t get_next_cluster(uint32_t cluster);
void set_next_cluster(uint32_t cluster, uint32_t value);
void fat_flush();
void fat_cleanup();
void format_filename_fat(const char *in, char *out11);
char **format_folder_path(const char *in);
bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out, bool *is_dir_out);
uint32_t get_fat_entry(uint32_t cluster);
uint32_t find_directory_entry_cluster(uint32_t dir_cluster, const char *name11);
void itos(int num, char *str)
{
    int i = 0;
    do
    {
        str[i] = '0' + (num % 10);
        num /= 10;
        i++;
    } while (num > 0);
    str[i] = '\0';
}
void stoi(char *str, int *num)
{
    *num = 0;
    while (*str >= '0' && *str <= '9')
    {
        *num = *num * 10 + (*str - '0');
        str++;
    }
}

void uint_to_str(uint32_t num, char *buf, size_t bufsize)
{
    if (bufsize == 0)
        return;

    // Заповнюємо весь буфер нулями
    for (size_t i = 0; i < bufsize; i++)
        buf[i] = 0;

    // Кінцевий нульовий символ
    buf[bufsize - 1] = '\0';

    if (num == 0)
    {
        if (bufsize > 1)
            buf[0] = '0';
        return;
    }

    int i = bufsize - 2; // індекс для останнього символу числа

    // Записуємо цифри з кінця
    while (num > 0 && i >= 0)
    {
        buf[i] = '0' + (num % 10);
        num /= 10;
        i--;
    }

    // Зсуваємо рядок в початок буфера
    int start = i + 1;
    int j = 0;
    while (buf[start] != '\0' && j < (int)bufsize)
    {
        buf[j++] = buf[start++];
    }
    buf[j] = '\0';
}

int to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

int fat32_init_from_lba(gpt_partition_t part)
{
    printf("🔎 Mounting FAT32 at LBA %d\n", part.first_lba);
    fat_start_lba = part.first_lba + 1;

    uint8_t sector[512];
    ata_read_sector(part.first_lba, sector);

    if (!(sector[510] == 0x55 && sector[511] == 0xAA))
    {
        printf("❌ Invalid FAT32 signature: 0x%X 0x%X\n", sector[510], sector[511]);
        return -1;
    }

    bpb = (FAT32_BPB *)malloc(sizeof(FAT32_BPB));
    memcpy(bpb, sector + 0x0B, sizeof(FAT32_BPB));
    total_fat_entries = (bpb->fat_size_32 * bpb->bytes_per_sector) / 4;
    cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    root_cluster = bpb->root_cluster;
    cluster_heap_lba = fat_start_lba + bpb->num_fats * bpb->fat_size_32;

    uint32_t fat_size_bytes = bpb->fat_size_32 * bpb->bytes_per_sector;
    fat_cache = malloc(fat_size_bytes);
    if (!fat_cache)
        return -2;

    for (uint32_t i = 0; i < bpb->fat_size_32; i++)
    {
        ata_read_sector(fat_start_lba + i, fat_cache + i * bpb->bytes_per_sector);
    }

    return 0;
}

uint32_t cluster_to_lba(uint32_t cluster)
{
    return cluster_heap_lba + (cluster - 2) * bpb->sectors_per_cluster;
}

void fat32_read_cluster(uint32_t cluster, uint8_t *buffer)
{
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t i = 0; i < bpb->sectors_per_cluster; i++)
    {
        ata_read_sector(lba + i, buffer + i * bpb->bytes_per_sector);
    }
}

void fat32_write_cluster(uint32_t cluster, uint8_t *buffer)
{
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t i = 0; i < bpb->sectors_per_cluster; i++)
    {
        ata_write_sector(lba + i, buffer + i * bpb->bytes_per_sector);
    }
}

uint32_t get_next_cluster(uint32_t cluster)
{
    return ((uint32_t *)fat_cache)[cluster] & 0x0FFFFFFF;
}

void set_next_cluster(uint32_t cluster, uint32_t value)
{
    ((uint32_t *)fat_cache)[cluster] = value & 0x0FFFFFFF;
    fat_dirty = true;
}

void fat_flush()
{
    if (!fat_dirty)
        return;
    for (uint32_t i = 0; i < bpb->num_fats; i++)
    {
        uint32_t base = fat_start_lba + i * bpb->fat_size_32;
        for (uint32_t s = 0; s < bpb->fat_size_32; s++)
        {
            ata_write_sector(base + s, fat_cache + s * bpb->bytes_per_sector);
        }
    }
    fat_dirty = false;
}

void fat_cleanup()
{
    if (fat_cache)
    {
        fat_flush();
        free(fat_cache);
        fat_cache = NULL;
    }
    if (bpb)
    {
        free(bpb);
        bpb = NULL;
    }
}

void format_filename_fat(const char *in, char *out11)
{
    int i = 0, j = 0;
    // Заповнити пробілами
    for (int k = 0; k < 11; ++k)
        out11[k] = ' ';

    while (in[i] && j < 11)
    {
        if (in[i] == '.')
        {
            j = 8; // після крапки йде розширення
            i++;
            continue;
        }

        if (j < 11)
            out11[j++] = to_upper(in[i]);
        i++;
    }
}

char **format_folder_path(const char *in)
{
    static char *out[16]; // макс 15 сегментів + 1 для лічильника
    int count = 0;

    const char *start = in;
    while (*start == '/')
        start++; // Пропустити початкові '/'

    while (*start && count < 15)
    {
        const char *end = start;
        while (*end && *end != '/')
            end++;

        int len = end - start;
        if (len > 0)
        {
            char name[12] = {0};
            for (int i = 0; i < len && i < 11; i++)
                name[i] = start[i];
            out[count + 1] = kmalloc(11);
            format_filename_fat(name, out[count + 1]);
            count++;
        }

        start = end;
        while (*start == '/')
            start++;
    }

    out[0] = kmalloc(2);
    itos(count, out[0]);
    return out;
}

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
            return 0; // не знайдено або кінець ланцюга
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
typedef struct
{
    char *buffer;
    size_t pos;
} list_ctx_t;

void list_files_callback(const char *name, bool is_dir, void *ctx_ptr)
{
    list_ctx_t *ctx = (list_ctx_t *)ctx_ptr;
    printf("prev_buf: %s\n", ctx->buffer);
    // without sprintf
    ctx->buffer[ctx->pos++] = is_dir ? '/' : ' ';
    memcpy(ctx->buffer + ctx->pos, name, strlen(name));
    ctx->pos += strlen(name);
    ctx->buffer[ctx->pos++] = '\n';
    printf("pos: %d\n", ctx->pos);
    printf("buffer: %s\n", ctx->buffer);
}

void fat32_list_files(uint32_t cluster, char *out_buf)
{
    memset(out_buf, 0, 2048);
    list_ctx_t ctx = {.buffer = out_buf, .pos = 0};
    iterate_directory(cluster, list_files_callback, &ctx);
    printf("%d", ctx.pos);
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
bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out, bool *is_dir_out)
{
    if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
        return false; // Кінець або видалено

    if ((entry->attr & 0x0F) == 0x0F)
        return false; // LFN не підтримується тут

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
void fat32_free_cluster(uint32_t cluster)
{
    if (cluster < 2 || cluster >= total_fat_entries)
    {
        printf("❌ Invalid cluster number: %u\n", cluster);
        return;
    }

    // Позначити кластер як вільний (0x00000000)
    fat_cache[cluster] = 0x00000000;
    fat_dirty = true; // Встановлюємо прапорець, що FAT змінився

    printf("🗑️ Cluster %u marked as free\n", cluster);
}

uint32_t fat32_allocate_cluster()
{
    for (uint32_t i = 2; i < total_fat_entries; ++i)
    {
        if (get_fat_entry(i) == 0x00000000) // 0 = free
        {
            set_fat_entry(i, 0x0FFFFFFF); // Mark as end-of-chain
            return i;
        }
    }

    return 0; // No free cluster found
}

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
    uint32_t parent_cluster = resolve_path_to_cluster(path);
    if (parent_cluster == 0)
    {
        printf("❌ Parent path not found: %s\n", path);
        return false;
    }

    // Виділяємо новий кластер для каталогу
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0)
    {
        printf("❌ No free clusters\n");
        return false;
    }

    // Ініціалізуємо новий каталог
    fat32_format_directory_cluster(new_cluster, parent_cluster);

    // Створюємо запис у батьківському каталозі
    FAT32_DirectoryEntry entry = {0};
    memcpy(entry.name, dirname11, 11); // Уже у 11-символьному форматі
    entry.attr = 0x10;                 // Атрибут: каталог
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.file_size = 0;

    bool result = fat32_add_directory_entry(parent_cluster, &entry);
    if (!result)
    {
        printf("❌ Failed to add directory entry\n");
        fat32_free_cluster(new_cluster); // очищаємо, якщо не додалося
        return false;
    }

    printf("📁 Directory created: %s in %s (cluster %d)\n", dirname11, path, new_cluster);
    return true;
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