
#include "utils/fat32/fat_structs.h"
#include "utils/fat32/fat_utils.h"
#include "utils/fat32/fat.h"
#include "utils/ata/ata.h"
#include "utils/vfs/vfs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

void itos(int num, char *str);
void stoi(char *str, int *num);
void uint_to_str(uint32_t num, char *buf, size_t bufsize);

// void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr);
// uint32_t cluster_to_lba(FAT32_FS *fs, uint32_t cluster);
// void fat32_read_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer);
// void ata_write_cluster(uint32_t cluster, const uint8_t *data);
// uint32_t get_next_cluster(FAT32_FS *fs, uint32_t cluster);
// void set_next_cluster(FAT32_FS *fs, uint32_t cluster, uint32_t value);
// void fat_flush();
// void fat_cleanup();
// void format_filename_fat(const char *in, char *out11);
// bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out, bool *is_dir_out);
// uint32_t get_fat_entry(uint32_t cluster);
// uint32_t find_directory_entry_cluster(uint32_t dir_cluster, const char *name11);

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
bool fat32_mount(FAT32_FS *fs, VFS_Device *device, uint32_t start_lba)
{
	fs->device = device->device;
	fs->start_lba = start_lba;
	fs->read_sector = device->read;
	fs->write_sector = device->write;

	fat32_init_from_lba(start_lba, fs);

	printf("here is root cluster: %d\n", fs->root_cluster);
	return true;
}

bool fat32_unmount(FAT32_FS *fs)
{
	fat_cleanup(fs);
	free(fs);
	return true;
}
FAT32_File *fat32_open(FAT32_FS *fs, const char *path)
{
	uint32_t cluster = resolve_path_to_cluster(fs, path);
	PathParts parts = format_folder_path(path);
	uint8_t *buf = malloc(fs->cluster_size);
	if (!buf)
	{
		printf("Failed to allocate buffer\n");
		return NULL;
	}
	fat32_read_cluster(fs, cluster, buf);
	for (int i = 0; i < fs->cluster_size / sizeof(FAT32_DirectoryEntry); i++)
	{
		FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));
		if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
			continue;
		if (memcmp(entry->name, parts.parts[parts.count - 1].sfn, 11) == 0)
		{
			printf("Found entry: %s\n", parts.parts[parts.count - 1].lfn);
			FAT32_File *file = malloc(sizeof(VFS_File));
			file->entry = entry;
			file->cluster = cluster;
			file->index = i;
			free(buf);
			return file;
		}
	}
	free(buf);
	return NULL;
}
int fat32_read(VFS_File *file, uint8_t *buffer, uint32_t size)
{
	FAT32_File *fat_file = file->node->fs_node;
	FAT32_DirectoryEntry *entry = fat_file->entry;
	FAT32_FS *fs = (FAT32_FS *)file->node->fs->fs;

	// printf("cluster: %d\n", fat_file->cluster);
	// printf("index: %d\n", fat_file->index);
	// printf("pos: %d\n", file->pos);

	size_t file_size = entry->file_size;
	if (file->pos >= file_size)
	{
		printf("EOF\n");
		return 0; // EOF
	}

	size_t to_read = (file->pos + size > file_size) ? (file_size - file->pos) : size;
	size_t read = 0;

	uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
	uint32_t cluster_offset = file->pos / fs->cluster_size;
	uint32_t in_cluster_offset = file->pos % fs->cluster_size;

	for (uint32_t i = 0; i < cluster_offset && cluster < 0x0FFFFFF8; i++)
	{
		cluster = get_fat_entry(fs, cluster);
	}
	// printf("cluster: %d\n", cluster);
	// printf("in_cluster_offset: %d\n", in_cluster_offset);
	// printf("to_read: %d\n", to_read);

	while (read < to_read && cluster < 0x0FFFFFF8)
	{
		uint8_t *cluster_buf = malloc(fs->cluster_size);
		if (!cluster_buf)
			return -1;

		fat32_read_cluster(fs, cluster, cluster_buf);
		// display raw data
		// printf("Cluster readed: %d\n", cluster);
		// uint16_t *buf = (uint16_t *)cluster_buf;
		// for (int j = 0; j < 16; j++)
		//	printf("%02X ", buf[j]);
		size_t available = fs->cluster_size - in_cluster_offset;
		size_t chunk = (to_read - read < available) ? (to_read - read) : available;

		memcpy(buffer + read, cluster_buf + in_cluster_offset, chunk);
		read += chunk;

		free(cluster_buf);
		cluster = get_fat_entry(fs, cluster);

		in_cluster_offset = 0; // після першого кластера завжди читаємо з початку
	}

	file->pos += read;
	return read;
}

int fat32_write(VFS_File *file, const uint8_t *buffer, uint32_t size)
{
	FAT32_File *fat_file = (FAT32_File *)file->node->fs_node;
	FAT32_DirectoryEntry *entry = fat_file->entry;
	FAT32_FS *fs = (FAT32_FS *)file->node->fs->fs;

	// 	printf("fat file: %p\n", fat_file);
	// 	printf("entry: %p\n", entry);
	// 	printf("cluster: %d\n", fat_file->cluster);
	// printf("index: %d\n", fat_file->index);

	uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;

	if (cluster == 0)
	{
		// файл порожній, треба виділити перший кластер
		cluster = fat32_allocate_cluster(fs);
		if (cluster == 0)
		{
			printf("No space for new file\n");
			return -1;
		}
		entry->first_cluster_low = cluster & 0xFFFF;
		entry->first_cluster_high = (cluster >> 16) & 0xFFFF;
	}

	size_t buf_offset = 0;
	size_t remaining = size;

	while (remaining > 0 && cluster < 0x0FFFFFF8)
	{
		uint8_t *cluster_buf = malloc(fs->cluster_size);
		fat32_read_cluster(fs, cluster, cluster_buf);
		size_t cluster_offset = (file->pos / fs->cluster_size);
		size_t space_in_cluster = fs->cluster_size - cluster_offset;
		size_t to_write = (remaining < space_in_cluster) ? remaining : space_in_cluster;

		memcpy(cluster_buf + cluster_offset, buffer + buf_offset, to_write);
		fat32_write_cluster(fs, cluster, cluster_buf);

		free(cluster_buf);

		remaining -= to_write;
		buf_offset += to_write;
		file->pos += to_write;

		if (remaining > 0)
		{
			uint32_t next = get_fat_entry(fs, cluster);
			if (next >= 0x0FFFFFF8)
			{
				next = fat32_allocate_cluster(fs);
				if (next == 0)
				{
					printf("No space during write\n");
					fat_flush(fs);
					return buf_offset; // скільки реально записали
				}
				set_fat_entry(fs, cluster, next);
			}
			cluster = next;
		}
	}
	// Оновлюємо розмір файлу
	if (file->pos > entry->file_size)
	{
		entry->file_size = file->pos;
	}
	file->node->size = entry->file_size;
	fat32_update_fat_entry(fs, fat_file);
	fat_flush(fs);
	return buf_offset;
}

int fat32_mkdir(FAT32_FS *fs, const char *path)
{
	fat32_create_directory(fs, path);
}
int fat32_delete(FAT32_FS *fs, const char *path)
{
	FAT32_File *entry = fat32_open(fs, path);
	if (entry->entry->attr & 0x10)
	{
		printf("Deleting directory: %s\n", path);
		fat32_delete_directory(fs, path);
	}
	else
	{
		printf("Deleting file: %s\n", path);
		fat32_delete_file(fs, path);
	}
	return 0;
}
int fat32_init_from_lba(uint32_t first_lba, FAT32_FS *fs)
{
	printf("🔎 Mounting FAT32 at LBA %d\n", first_lba);
	uint8_t sector[512];
	printf("Reading FAT32 signature\n");
	fs->read_sector(fs->device, first_lba, sector);
	printf("FAT32 signature: 0x%X 0x%X\n", sector[510], sector[511]);
	if (!(sector[510] == 0x55 && sector[511] == 0xAA))
	{
		printf("Invalid FAT32 signature: 0x%X 0x%X\n", sector[510], sector[511]);
		return -1;
	}

	FAT32_BPB *bpb = malloc(sizeof(FAT32_BPB));
	if (!bpb)
	{
		printf("Failed to allocate memory for BPB\n");
		return -2;
	}
	memcpy(bpb, sector + 0x0B, sizeof(FAT32_BPB));

	fs->total_sectors = bpb->total_sectors_32;
	fs->sectors_per_cluster = bpb->sectors_per_cluster;
	fs->cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
	fs->total_fat_entries = (bpb->fat_size_32 * bpb->bytes_per_sector) / 4;
	fs->bytes_per_sector = bpb->bytes_per_sector;

	fs->reserved_sectors = bpb->reserved_sector_count;
	fs->num_fats = bpb->num_fats;
	fs->sectors_per_fat = bpb->fat_size_32;
	fs->root_cluster = bpb->root_cluster;

	fs->fat_start_lba = first_lba + bpb->reserved_sector_count;
	fs->cluster_heap_lba = fs->fat_start_lba + bpb->num_fats * bpb->fat_size_32;
	fs->fat_size_32 = bpb->fat_size_32;

	// print all
	printf("Total sectors: %d\n", fs->total_sectors);
	printf("Sectors per cluster: %d\n", fs->sectors_per_cluster);
	printf("Cluster size: %d\n", fs->cluster_size);
	printf("Total FAT entries: %d\n", fs->total_fat_entries);
	printf("Bytes per sector: %d\n", fs->bytes_per_sector);
	printf("Reserved sectors: %d\n", fs->reserved_sectors);
	printf("Number of FATs: %d\n", fs->num_fats);
	printf("Sectors per FAT: %d\n", fs->sectors_per_fat);
	printf("Root cluster: %d\n", fs->root_cluster);
	printf("Cluster heap LBA: %d\n", fs->cluster_heap_lba);

	uint32_t fat_size_bytes = bpb->fat_size_32 * bpb->bytes_per_sector;
	printf("FAT size in bytes: %d\n", fat_size_bytes);

	fs->fat_cache = malloc(fat_size_bytes);
	if (!fs->fat_cache)
	{
		free(bpb);
		printf("Failed to allocate memory for FAT cache\n");
		return -3;
	}

	for (uint32_t i = 0; i < bpb->fat_size_32; i++)
	{
		fs->read_sector(fs->device, fs->fat_start_lba + i,
				((uint8_t *)(fs->fat_cache) + i * bpb->bytes_per_sector));
	}

	fs->fat_dirty = false;
	free(bpb); // звільняємо тільки тут, після використання
	return 0;
}

uint32_t cluster_to_lba(FAT32_FS *fs, uint32_t cluster)
{
	return fs->cluster_heap_lba + (cluster - 2) * fs->sectors_per_cluster;
}
int fat32_update_fat_entry(FAT32_FS *fs, FAT32_File *file)
{
	uint8_t *buf = malloc(fs->cluster_size);
	if (!buf)
		return -1;

	fat32_read_cluster(fs, file->cluster, buf);

	FAT32_DirectoryEntry *entries = (FAT32_DirectoryEntry *)buf;

	memcpy(&entries[file->index], file->entry, sizeof(FAT32_DirectoryEntry));

	fat32_write_cluster(fs, file->cluster, buf);
	free(buf);
	return 0;
}

void fat32_read_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer)
{
	uint32_t lba = cluster_to_lba(fs, cluster);
	for (uint32_t i = 0; i < fs->sectors_per_cluster; i++)
	{
		// printf("lba: %d\n", lba + i);
		fs->read_sector(fs->device, lba + i, buffer + i * fs->bytes_per_sector);
	}
}

void fat32_write_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer)
{
	for (int i = 0; i < 20; i++)
	{
		printf("%c", buffer[i]);
	}
	uint32_t lba = cluster_to_lba(fs, cluster);
	for (uint32_t i = 0; i < fs->sectors_per_cluster; i++)
	{
		fs->write_sector(fs->device, lba + i, buffer + i * fs->bytes_per_sector);
	}

	printf("Wrote cluster %d\n", cluster);
}

// uint32_t get_next_cluster(FAT32_FS *fs, uint32_t cluster)
// {
//     return ((uint32_t *)(fs->fat_cache))[cluster] & 0x0FFFFFFF;
// }

// void set_next_cluster(FAT32_FS *fs, uint32_t cluster, uint32_t value)
// {
//     ((uint32_t *)(fs->fat_cache))[cluster] = value & 0x0FFFFFFF;
//     fs->fat_dirty = true;
// }

bool fat_flush(FAT32_FS *fs)
{
	if (!fs->fat_dirty || !fs->fat_cache)
		return true;

	uint32_t fat_size_sectors = fs->fat_size_32;
	for (int f = 0; f < fs->num_fats; ++f)
	{
		uint32_t base = fs->fat_start_lba + f * fat_size_sectors;
		for (uint32_t s = 0; s < fat_size_sectors; ++s)
		{
			if (!fs->write_sector(fs->device, base + s,
					      ((uint8_t *)fs->fat_cache) + s * fs->bytes_per_sector))
			{
				return false; // error!
			}
		}
	}
	fs->fat_dirty = false;
	return true;
}

void fat_cleanup(FAT32_FS *fs)
{
	if (fs->fat_cache)
	{
		fat_flush(fs);
		free(fs->fat_cache);
		fs->fat_cache = NULL;
	}
}

void format_filename_fat(const char *in, char out11[12])
{
	int i = 0, j = 0;
	char temp_out[12];
	memset(temp_out, ' ', 12);

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
	temp_out[12] = '\0';

	for (i = 0; i < 12; i++)
	{
		out11[i] = temp_out[i];
	}
}

PathParts format_folder_path(const char *in)
{
	PathParts result = {0};

	while (*in == '/')
		in++; // пропустити початкові '/'

	while (*in && result.count < MAX_PARTS && *in != '\0')
	{
		const char *end = in;
		while (*end && *end != '/' && *end != '\0')
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
		while (*in == '/' && *in != '\0')
			in++;
	}
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
int fat32_create_entry(FAT32_FS *fs, uint32_t cluster, PathPart *pp, bool is_dir)
{
	printf("sfn: %s\n", pp->sfn);
	if (cluster == 0)
	{
		printf("Cluster not found: %s\n", pp->lfn);
		return -EINVAL;
	}
	if (find_directory_entry_cluster(fs, cluster, pp->sfn) != 0)
	{
		printf("Entry already exists: %s\n", pp->lfn);
		return -EEXIST;
	}
	uint8_t *buf = malloc(fs->cluster_size);
	if (!buf)
	{
		printf("Failed to allocate buffer\n");
		return -ENOMEM;
	}
	fat32_read_cluster(fs, cluster, buf);

	FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)buf;
	for (int i = 0; i < fs->cluster_size / sizeof(FAT32_DirectoryEntry); i++, entry++)
	{

		if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
		{

			memcpy(entry->name, pp->sfn, 11);

			printf("Entry created and found: %s\n", pp->sfn);

			entry->attr = is_dir ? 0x10 : 0x20;
			uint32_t new_cluster = fat32_allocate_cluster(fs);
			entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
			entry->first_cluster_low = new_cluster & 0xFFFF;

			entry->file_size = 0;
			fat32_write_cluster(fs, cluster, buf);

			free(buf);

			// add entry to directory
			if (is_dir)
				fat32_format_directory_cluster(fs, new_cluster, cluster);
			fat_flush(fs);
			return 0;
		}
	}

	free(buf);
	return -ENOSPC; // нема місця в директорії
}
int fat32_delete_entry(FAT32_FS *fs, uint32_t cluster, const char *name)
{
	if (cluster == 0)
	{
		printf("Cluster not found: %s\n", name);
		return -EINVAL;
	}
	if (find_directory_entry_cluster(fs, cluster, name) == 0)
	{
		printf("Entry not found: %s\n", name);
		return -ENOENT;
	}
	uint8_t *buf = malloc(fs->cluster_size);
	if (!buf)
	{
		printf("Failed to allocate buffer\n");
		return -ENOMEM;
	}
	fat32_read_cluster(fs, cluster, buf);
	FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)buf;
	for (int i = 0; i < fs->cluster_size / sizeof(FAT32_DirectoryEntry); i++, entry++)
	{
		if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
			continue;
		if (memcmp(entry->name, name, 11) == 0)
		{
			entry->name[0] = 0xE5;
			fat32_write_cluster(fs, cluster, buf);
			free(buf);
			fat_flush(fs);
			return 0;
		}
	}
	free(buf);
	return -ENOENT;
}
