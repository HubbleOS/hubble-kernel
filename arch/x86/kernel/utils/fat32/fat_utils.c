
#include "utils/fat32/fat_structs.h"
#include "utils/fat32/fat_utils.h"
#include "utils/ata/ata.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void itos(int num, char *str);
void stoi(char *str, int *num);
void uint_to_str(uint32_t num, char *buf, size_t bufsize);

void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr);
uint32_t cluster_to_lba(uint32_t cluster);
void fat32_read_cluster(uint32_t cluster, uint8_t *buffer);
void ata_write_cluster(uint32_t cluster, const uint8_t *data);
uint32_t get_next_cluster(uint32_t cluster);
void set_next_cluster(uint32_t cluster, uint32_t value);
void fat_flush();
void fat_cleanup();
void format_filename_fat(const char *in, char *out11);
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

    for (size_t i = 0; i < bufsize; i++)
        buf[i] = 0;

    buf[bufsize - 1] = '\0';

    if (num == 0)
    {
        if (bufsize > 1)
            buf[0] = '0';
        return;
    }

    int i = bufsize - 2;

    while (num > 0 && i >= 0)
    {
        buf[i] = '0' + (num % 10);
        num /= 10;
        i--;
    }

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
        printf("Invalid FAT32 signature: 0x%X 0x%X\n", sector[510], sector[511]);
        return -1;
    }

    bpb = malloc(sizeof(FAT32_BPB));
    if (!bpb)
    {
        printf("Failed to allocate memory for BPB\n");
        return -2;
    }
    memcpy(bpb, sector + 0x0B, sizeof(FAT32_BPB));

    total_fat_entries = (bpb->fat_size_32 * bpb->bytes_per_sector) / 4;
    cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    root_cluster = bpb->root_cluster;
    cluster_heap_lba = fat_start_lba + bpb->num_fats * bpb->fat_size_32;

    uint32_t fat_size_bytes = bpb->fat_size_32 * bpb->bytes_per_sector;
    fat_cache = malloc(fat_size_bytes);
    if (!fat_cache)
    {
        free(bpb);
        printf("Failed to allocate memory for FAT cache\n");
        return -3;
    }

    for (uint32_t i = 0; i < bpb->fat_size_32; i++)
    {
        ata_read_sector(fat_start_lba + i, ((uint8_t *)fat_cache) + i * bpb->bytes_per_sector);
    }

    fat_dirty = false; // Ініціалізуємо прапорець "чистоти" кеша
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

void fat_flush(void)
{
    if (!fat_dirty || !fat_cache)
        return;
    uint32_t fat_size_sectors = bpb->fat_size_32;
    for (int f = 0; f < bpb->num_fats; ++f)
    {
        uint32_t base = fat_start_lba + f * fat_size_sectors;
        for (uint32_t s = 0; s < fat_size_sectors; ++s)
        {
            ata_write_sector(base + s, ((uint8_t *)fat_cache) + s * bpb->bytes_per_sector);
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

void format_filename_fat(const char *in, char out11[12])
{
    int i = 0, j = 0;
    char temp_out[12] = {0};

    while (in[i] && j < 11)
    {
        if (in[i] == '.')
        {
            j = 8;
            i++;
            continue;
        }
        if (in[i] == ' ')
        {
            break;
        }
        temp_out[j++] = to_upper(in[i]);
        i++;
    }
    temp_out[11] = '\0';

    for (i = 0; i < 11; i++)
    {
        printf("%c", temp_out[i]);
        out11[i] = temp_out[i];
    }
}

PathParts format_folder_path(const char *in)
{
    PathParts result = {0};

    while (*in == '/')
        in++; // пропустити початкові '/'

    while (*in && result.count < MAX_PARTS)
    {
        const char *end = in;
        while (*end && *end != '/')
            end++;

        int len = end - in;
        if (len > 0)
        {
            char name[256] = {0};
            strncpy(name, in, len);

            // SFN
            format_filename_fat(name, result.parts[result.count].sfn);

            // LFN (оригінальне ім’я)
            result.parts[result.count].lfn = malloc(len + 1);
            memcpy(result.parts[result.count].lfn, name, len);
            result.parts[result.count].lfn[len] = '\0';

            result.count++;
        }

        in = end;
        while (*in == '/')
            in++;
    }
    printf("parts: %d\n", result.count);
    return result;
}

void free_folder_path(PathParts *pp)
{
    for (int i = 0; i < pp->count; i++)
    {
        free(pp->parts[i].lfn);
    }
    pp->count = 0;
}
bool fat32_create_entry(uint32_t cluster, PathPart *pp, bool is_dir)
{
    uint8_t *buf = malloc(cluster_size);
    fat32_read_cluster(cluster, buf);
    FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)buf;
    for (int i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++, entry++)
    {
        if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
        {
            // 3. Формуємо ім’я у форматі 8.3

            memcpy(entry->name, pp->sfn, 11);

            // 4. Записуємо атрибут
            entry->attr = is_dir ? 0x10 : 0x20;

            // 5. Виділяємо кластер для файлу/директорії
            uint32_t new_cluster = fat32_allocate_cluster();
            entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
            entry->first_cluster_low = new_cluster & 0xFFFF;

            // 6. Розмір (для директорії = 0)
            entry->file_size = 0;

            // 7. Записуємо назад директорію
            fat32_write_cluster(cluster, buf);
            free(buf);

            // 8. Для директорії створюємо "." і ".."
            if (is_dir)
                fat32_format_directory_cluster(new_cluster, cluster);
            fat_flush();
            return true;
        }
    }

    free(buf);
    return false; // нема місця в директорії
}
// FAT32_DirectoryEntry *get_directory_entry(uint32_t cluster, const char *name)
// {
//     uint8_t *buf = malloc(cluster_size);
//     fat32_read_cluster(cluster, buf);
//     return fat32_get_directory_entry(buf, name);