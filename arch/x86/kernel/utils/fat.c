#include "fat.h"
#include "utils/fat_structs.h"
#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define MAX_CLUSTER_CHAIN 1024

static uint8_t *fs_base;
static FAT32_BPB *bpb;
static uint32_t fat_start, cluster_heap_start, root_cluster;
static uint32_t cluster_size;
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

void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

int to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

int strcasecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (to_upper(*a) != to_upper(*b))
            return to_upper(*a) - to_upper(*b);
        a++;
        b++;
    }
    return to_upper(*a) - to_upper(*b);
}

int fat32_init(void *ramdisk_base)
{
    fs_base = (uint8_t *)ramdisk_base;
    bpb = (FAT32_BPB *)fs_base; // BPB починається з offset 0x0B

    fat_start = bpb->reserved_sector_count;
    cluster_heap_start = fat_start + bpb->num_fats * bpb->fat_size_32;
    root_cluster = bpb->root_cluster;
    cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    return 0;
}

void debug_fat32(framebuffer_info_t *fb)
{
    // char buf[4096];
    // print_text(fb, "FAT32", 0, 0, 0xFFFFFF);
    // print_text(fb, "FAT Start: ", 0, 20, 0xFFFFFF);
    // uint_to_str(fat_start, buf, sizeof(buf));
    // print_text(fb, buf, 150, 20, 0xFFFFFF);
    // print_text(fb, "Cluster Heap Start: ", 0, 40, 0xFFFFFF);
    // uint_to_str(cluster_heap_start, buf, sizeof(buf));
    // print_text(fb, buf, 150, 40, 0xFFFFFF);
    // print_text(fb, "Root Cluster: ", 0, 60, 0xFFFFFF);
    // uint_to_str(root_cluster, buf, sizeof(buf));
    // print_text(fb, buf, 150, 60, 0xFFFFFF);
    // print_text(fb, "Cluster Size: ", 0, 80, 0xFFFFFF);
    // uint_to_str(cluster_size, buf, sizeof(buf));
    // print_text(fb, buf, 150, 80, 0xFFFFFF);
}

static uint8_t *get_cluster_ptr(uint32_t cluster)
{
    uint32_t offset = cluster_heap_start + (cluster - 2) * bpb->sectors_per_cluster;
    return fs_base + offset * bpb->bytes_per_sector;
}

static uint32_t get_next_cluster(uint32_t cluster)
{
    uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
    return fat[cluster] & 0x0FFFFFFF;
}
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    for (size_t i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
            return (int)p1[i] - (int)p2[i];
    }

    return 0;
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

uint32_t fat32_find_dir_in(uint32_t start_cluster, const char *target_name)
{
    uint32_t cluster = start_cluster;
    int cluster_steps = 0;

    while (cluster < 0x0FFFFFF8 && cluster_steps++ < MAX_CLUSTER_CHAIN)
    {
        uint8_t *cluster_ptr = get_cluster_ptr(cluster);
        if (!cluster_ptr)
            return 0;

        for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));

            if (entry->name[0] == 0x00)
                return 0; // кінець

            if ((entry->attr & 0x0F) == 0x0F)
                continue; // long name

            if (!(entry->attr & 0x10))
                continue; // не директорія

            if (memcmp(entry->name, target_name, 11) == 0)
            {
                return (entry->first_cluster_high << 16) | entry->first_cluster_low;
            }
        }

        cluster = get_next_cluster(cluster);
    }

    return 0;
}
uint32_t fat32_find_dir(char **path)
{
    int count = atoi(path[0]);

    uint32_t cluster = root_cluster;
    if (count < 1)
    {
        return root_cluster;
    }
    for (int i = 1; i <= count; i++)
    {

        cluster = fat32_find_dir_in(cluster, path[i]);
        if (cluster == 0)
            return 0;
    }

    return cluster;
}

FAT32_DirectoryEntry *fat32_find_file_in(uint32_t start_cluster, const char *target_name)
{
    uint32_t cluster = start_cluster;
    int cluster_steps = 0;

    while (cluster < 0x0FFFFFF8 && cluster_steps++ < MAX_CLUSTER_CHAIN)
    {
        uint8_t *cluster_ptr = get_cluster_ptr(cluster);
        if (!cluster_ptr)
            return 0;

        for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));

            if (entry->name[0] == 0x00)
                return 0; // кінець директорії

            if ((entry->attr & 0x0F) == 0x0F)
                continue; // long name

            if (entry->attr & 0x10)
                continue; // пропускаємо директорії

            if (memcmp(entry->name, target_name, 11) == 0)
                return entry;
        }

        cluster = get_next_cluster(cluster);
    }

    return 0;
}

int fat32_read_file(const char *path, void *out_buf, size_t *out_size)
{
    char **folder_path = format_folder_path(path);
    int depth = atoi(folder_path[0]);
    uint32_t dir_cluster = 0;
    if (depth <= 1)
        dir_cluster = root_cluster;
    else
        dir_cluster = fat32_find_dir(folder_path);
    if (dir_cluster < 2)
        return -1;

    char *target_name = folder_path[depth];
    FAT32_DirectoryEntry *entry = fat32_find_file_in(dir_cluster, target_name);

    if (!entry)
        return -1;

    uint32_t file_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    uint32_t bytes_read = 0;
    uint8_t *out = (uint8_t *)out_buf;

    while (file_cluster < 0x0FFFFFF8)
    {
        uint8_t *src = get_cluster_ptr(file_cluster);
        if (!src)
            return -1;

        uint32_t to_copy = cluster_size;
        if (bytes_read + to_copy > entry->file_size)
            to_copy = entry->file_size - bytes_read;

        memcpy(out + bytes_read, src, to_copy);
        bytes_read += to_copy;

        if (bytes_read >= entry->file_size)
            break;

        file_cluster = get_next_cluster(file_cluster);
    }

    if (out_size)
        *out_size = bytes_read;
    return 0; // успішне прочитання
}

int fat32_write_file(const char *path, const void *data, size_t size)
{
    char **folder_path = format_folder_path(path);
    int depth = atoi(folder_path[0]);
    uint32_t dir_cluster = 0;
    if (depth <= 1)
        dir_cluster = root_cluster;
    else
        dir_cluster = fat32_find_dir(folder_path);
    if (dir_cluster < 2)
        return -1;

    char *target = folder_path[depth];

    uint32_t cluster = dir_cluster;
    int cluster_steps = 0;

    FAT32_DirectoryEntry *existing_entry = NULL;
    uint8_t *cluster_ptr = NULL;

    // Шукаємо існуючий файл або вільний запис у директорії
    while (cluster < 0x0FFFFFF8 && cluster_steps++ < MAX_CLUSTER_CHAIN)
    {
        cluster_ptr = get_cluster_ptr(cluster);
        if (!cluster_ptr)
            return -2;

        for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));

            if (memcmp(entry->name, target, 11) == 0)
            {
                existing_entry = entry;
                goto found_entry;
            }
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
            {
                if (!existing_entry)
                    existing_entry = entry; // вільний запис
            }
        }

        cluster = get_next_cluster(cluster);
    }

found_entry:

    if (!existing_entry)
        return -3; // Нема вільного запису і немає існуючого файлу

    // Якщо файл існує, звільняємо кластери у FAT
    if (existing_entry->name[0] != 0x00 && existing_entry->name[0] != 0xE5)
    {
        uint32_t file_cluster = (existing_entry->first_cluster_high << 16) | existing_entry->first_cluster_low;
        uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);

        while (file_cluster < 0x0FFFFFF8 && file_cluster >= 2)
        {
            uint32_t next = fat[file_cluster] & 0x0FFFFFFF;
            fat[file_cluster] = 0; // звільнити кластер
            if (next >= 0x0FFFFFF8)
                break;
            file_cluster = next;
        }
    }

    // Знайти вільні кластери для нового файлу
    uint32_t needed_clusters = (size + cluster_size - 1) / cluster_size;
    uint32_t first_cluster = 0, prev_cluster = 0;
    uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
    uint32_t fat_entries = bpb->fat_size_32 * bpb->bytes_per_sector / 4;
    uint32_t found = 0;

    for (uint32_t i = 2; i < fat_entries && found < needed_clusters; i++)
    {
        if ((fat[i] & 0x0FFFFFFF) == 0)
        {
            fat[i] = 0x0FFFFFFF; // кінець ланцюга поки що
            if (prev_cluster != 0)
                fat[prev_cluster] = i;
            else
                first_cluster = i;
            prev_cluster = i;
            found++;
        }
    }

    if (found < needed_clusters)
        return -4; // Недостатньо місця

    // Записуємо дані у кластери
    const uint8_t *src = (const uint8_t *)data;
    uint32_t bytes_written = 0;
    uint32_t current_cluster = first_cluster;

    while (bytes_written < size && current_cluster < 0x0FFFFFF8)
    {
        uint8_t *dest = get_cluster_ptr(current_cluster);
        if (!dest)
            return -5;

        uint32_t to_copy = cluster_size;
        if (bytes_written + to_copy > size)
            to_copy = size - bytes_written;

        memcpy(dest, src + bytes_written, to_copy);
        bytes_written += to_copy;

        current_cluster = get_next_cluster(current_cluster);
    }

    // Оновлюємо директорний запис
    memcpy(existing_entry->name, target, 11);
    existing_entry->attr = 0x20; // файл
    existing_entry->first_cluster_low = first_cluster & 0xFFFF;
    existing_entry->first_cluster_high = (first_cluster >> 16) & 0xFFFF;
    existing_entry->file_size = size;

    return 0;
}

int fat32_delete_file(const char *filename)
{
    char **folder_path = format_folder_path(filename);
    int depth = atoi(folder_path[0]);
    uint32_t dir_cluster = 0;
    if (depth <= 1)
        dir_cluster = root_cluster;
    else
        dir_cluster = fat32_find_dir(folder_path);
    if (dir_cluster < 2)
        return -1;

    char *target = folder_path[depth];

    FAT32_DirectoryEntry *entry = fat32_find_file_in(dir_cluster, target);
    if (!entry)
        return -1;

    // Позначаємо файл як видалений
    entry->name[0] = 0xE5;

    // Очищення кластерів
    uint32_t file_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);

    while (file_cluster < 0x0FFFFFF8)
    {
        uint32_t next = fat[file_cluster] & 0x0FFFFFFF;
        fat[file_cluster] = 0x00000000;
        file_cluster = next;
    }

    return 0;
}

int fat32_rename_file(const char *oldname, const char *newname)
{
    char **folder_path = format_folder_path(oldname);
    char *folder_path_new;
    format_filename_fat(newname, folder_path_new);
    int depth = atoi(folder_path[0]);
    uint32_t dir_cluster = 0;
    if (depth <= 1)
        dir_cluster = root_cluster;
    else
        dir_cluster = fat32_find_dir(folder_path);
    if (dir_cluster < 2)
        return -1;

    char *target = folder_path[depth];
    if (folder_path_new == folder_path[depth])
    {
        return 0;
    }

    FAT32_DirectoryEntry *entry = fat32_find_file_in(dir_cluster, target);
    if (!entry)
        return -2;
    memcpy(entry->name, folder_path_new, 11);
    return 0;
}
int fat32_list_files(const char *path, char *out_buf)
{
    char **folder_path = format_folder_path(path);
    int depth = atoi(folder_path[0]);
    uint32_t dir_cluster = root_cluster;
    if (depth >= 1)
    {
        dir_cluster = fat32_find_dir(folder_path);
        if (dir_cluster < 2)
            return -1;
    }

    uint32_t cluster = dir_cluster;
    int cluster_steps = 0;
    char *ptr = out_buf;

    while (cluster < 0x0FFFFFF8 && cluster_steps++ < MAX_CLUSTER_CHAIN)
    {
        uint8_t *cluster_ptr = get_cluster_ptr(cluster);
        if (!cluster_ptr)
            return 0;

        for (size_t i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));

            if (entry->name[0] == 0x00)
                return 0;

            if (entry->name[0] == 0xE5)
                continue;

            if ((entry->attr & 0x0F) == 0x0F)
                continue;

            // Скопіювати імʼя
            for (int j = 0; j < 11; j++)
            {
                char c = entry->name[j];
                if (c == ' ')
                    break;
                *ptr++ = c;
            }

            // Додати мітку: [D] - directory, [F] - file
            *ptr++ = ' ';
            *ptr++ = '[';
            *ptr++ = (entry->attr & 0x10) ? 'D' : 'F';
            *ptr++ = ']';
            *ptr++ = '\n';
        }

        cluster = get_next_cluster(cluster);
    }

    *ptr = '\0';
    return 0;
}
int fat32_create_folder(const char *path)
{
    char **folder_path = format_folder_path(path);
    int depth = atoi(folder_path[0]);
    uint32_t dir_cluster = root_cluster;
    if (depth - 1 >= 1)
    {
        dir_cluster = fat32_find_dir(folder_path);
        if (dir_cluster < 2)
            return -1;
    }

    uint32_t cluster = dir_cluster;
    FAT32_DirectoryEntry *free_entry = NULL;
    while (cluster < 0x0FFFFFF8)
    {
        uint8_t *cluster_ptr = get_cluster_ptr(cluster);
        for (int i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
            {
                free_entry = entry;
                goto found_free;
            }
        }
        cluster = get_next_cluster(cluster);
    }
    return -2;

found_free:
    uint32_t new_cluster = 0, prev = 0;
    uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
    for (uint32_t i = 2; i < bpb->fat_size_32 * bpb->bytes_per_sector / 4; i++)
    {
        if ((fat[i] & 0x0FFFFFFF) == 0)
        {
            fat[i] = 0x0FFFFFFF;
            new_cluster = i;
            break;
        }
    }
    if (new_cluster == 0)
        return -3;

    memset(get_cluster_ptr(new_cluster), 0, cluster_size);

    // Make directory
    memcpy(free_entry->name, folder_path[depth], 11);
    free_entry->attr = 0x10; // directory
    free_entry->first_cluster_low = new_cluster & 0xFFFF;
    free_entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    free_entry->file_size = 0;

    // Prepare to add `.`, `..`
    FAT32_DirectoryEntry *entries = (FAT32_DirectoryEntry *)get_cluster_ptr(new_cluster);

    // `.`
    memset(&entries[0], 0, sizeof(FAT32_DirectoryEntry));
    memcpy(entries[0].name, ".          ", 11);
    entries[0].attr = 0x10;
    entries[0].first_cluster_low = new_cluster & 0xFFFF;
    entries[0].first_cluster_high = (new_cluster >> 16) & 0xFFFF;

    // `..`
    memset(&entries[1], 0, sizeof(FAT32_DirectoryEntry));
    memcpy(entries[1].name, "..         ", 11);
    entries[1].attr = 0x10;
    entries[1].first_cluster_low = dir_cluster & 0xFFFF;
    entries[1].first_cluster_high = (dir_cluster >> 16) & 0xFFFF;

    return 0;
}

int fat32_delete_dir(const char *path)
{
    char **folder_path = format_folder_path(path);
    int depth = atoi(folder_path[0]);
    uint32_t parent_cluster = root_cluster;
    if (depth - 1 >= 1)
    {
        parent_cluster = fat32_find_dir(folder_path);
        if (parent_cluster < 2)
            return -1;
    }

    // get target
    char *target = folder_path[depth];

    // find dir
    uint32_t cluster = parent_cluster;
    FAT32_DirectoryEntry *dir_entry = NULL;

    while (cluster < 0x0FFFFFF8)
    {
        uint8_t *cluster_ptr = get_cluster_ptr(cluster);
        for (int i = 0; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
        {
            FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(cluster_ptr + i * sizeof(FAT32_DirectoryEntry));
            if (entry->name[0] == 0x00)
                break;

            if ((entry->attr & 0x0F) == 0x0F)
                continue;

            if ((entry->attr & 0x10) && memcmp(entry->name, target, 11) == 0)
            {
                dir_entry = entry;
                goto found_dir;
            }
        }
        cluster = get_next_cluster(cluster);
    }
    return -3; // not found

found_dir:
    // check if dir is empty
    uint32_t dir_cluster = (dir_entry->first_cluster_high << 16) | dir_entry->first_cluster_low;
    uint8_t *cluster_ptr = get_cluster_ptr(dir_cluster);
    FAT32_DirectoryEntry *entries = (FAT32_DirectoryEntry *)cluster_ptr;

    for (int i = 2; i < cluster_size / sizeof(FAT32_DirectoryEntry); i++)
    {
        if (entries[i].name[0] == 0x00)
            break; // end
        if (entries[i].name[0] != 0xE5)
            return -4; // dir is not empty
    }

    // Очистити FAT
    uint32_t *fat = (uint32_t *)(fs_base + fat_start * bpb->bytes_per_sector);
    uint32_t next = fat[dir_cluster] & 0x0FFFFFFF;
    fat[dir_cluster] = 0;
    while (next < 0x0FFFFFF8)
    {
        uint32_t temp = fat[next] & 0x0FFFFFFF;
        fat[next] = 0;
        next = temp;
    }

    // Видалити запис у батьківській директорії
    dir_entry->name[0] = 0xE5;

    return 0;
}