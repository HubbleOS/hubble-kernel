#include "fat_structs.h"
#include "fat_utils.h"
#include "fat.h"
#include "printk.h"

#include <mm/kmalloc.h>

#include <fs/ata/ata.h>
#include <fs/vfs/vfs.h>

#include <string.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>

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

bool fat32_mount(FAT32_FS *fs, VFS_Device *device, uint32_t start_lba)
{
	fs->device = device->device;
	fs->start_lba = start_lba;
	fs->read_sector = device->read;
	fs->write_sector = device->write;

	fat32_init_from_lba(start_lba, fs);
	if (!fs)
	{
		printk("fs is null\n");
	}
	printk("here is root cluster: %d\n", fs->root_cluster);
	return true;
}

bool fat32_unmount(FAT32_FS *fs)
{
	fat_cleanup(fs);
	kfree(fs);
	return true;
}
#include "io.h"
FAT32_File *fat32_open(FAT32_FS *fs, const char *path)
{
	if (!fs)
	{
		printk("fs is null\n");
		return NULL;
	}

	printk("FAT32: opening file at path: %s\n", path);
	uint32_t cluster = resolve_path_to_cluster(fs, path);

	if (cluster == 0)
		return NULL;

	PathParts parts = format_folder_path(path);

	uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
	if (!buf)
		return NULL;

	fat32_read_cluster(fs, cluster, buf);
	if (!buf)
	{
		kfree(buf);
		return NULL;
	}

	printk("Cluster data open: ");
	for (int i = 0; i < fs->cluster_size / sizeof(FAT32_DirectoryEntry); i++)
	{
		FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)(buf + i * sizeof(FAT32_DirectoryEntry));

		if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
			continue;

		if (memcmp(entry->name, parts.parts[parts.count - 1].sfn, 11) == 0)
		{
			printk("File found\n");
			FAT32_File *file = kmalloc(sizeof(FAT32_File), GFP_KERNEL);
			if (!file)
			{
				printk("File not found 1\n");
				kfree(buf);
				return NULL;
			}

			file->entry = kmalloc(sizeof(FAT32_DirectoryEntry), GFP_KERNEL);
			if (!file->entry)
			{
				printk("File not found 2\n");
				kfree(buf);
				kfree(file);
				return NULL;
			}
			printk("Entry: ");
			for (int i = 0; i < 256; i++)
				outb(0x3f8, entry->name[i]);

			*(file->entry) = *entry; // копіюємо структуру
			file->cluster = cluster;
			file->index = i;

			kfree(buf);
			return file;
		}
	}

	kfree(buf);
	printk("File not found\n");
	return NULL;
}

int fat32_read(VFS_File *file, uint8_t *buffer, uint32_t size)
{
	FAT32_File *fat_file = file->node->fs_node;
	FAT32_DirectoryEntry *entry = fat_file->entry;
	FAT32_FS *fs = (FAT32_FS *)file->node->fs->fs;

	// printk("cluster: %d\n", fat_file->cluster);
	// printk("index: %d\n", fat_file->index);
	// printk("pos: %d\n", file->pos);

	size_t file_size = entry->file_size;
	if (file->pos >= file_size)
	{
		printk("EOF\n");
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

	// printk("cluster: %d\n", cluster);
	// printk("in_cluster_offset: %d\n", in_cluster_offset);
	// printk("to_read: %d\n", to_read);

	while (read < to_read && cluster < 0x0FFFFFF8)
	{
		printk("Reading cluster: %d ", cluster);
		uint8_t *cluster_buf = kmalloc(fs->cluster_size, GFP_KERNEL);
		if (!cluster_buf)
			return -1;

		fat32_read_cluster(fs, cluster, cluster_buf);
		// display raw data
		printk("Cluster readed: %d ", cluster);
		uint16_t *buf = (uint16_t *)cluster_buf;
		for (int j = 0; j < 16; j++)
			printk("%02X ", buf[j]);
		printk("\n");

		size_t available = fs->cluster_size - in_cluster_offset;
		size_t chunk = (to_read - read < available) ? (to_read - read) : available;

		memcpy(buffer + read, cluster_buf + in_cluster_offset, chunk);
		printk("Read chunk: %d bytes, readed: %d\n", chunk, read + chunk);
		read += chunk;
		kfree(cluster_buf);
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

	uint32_t cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;

	if (cluster == 0)
	{
		cluster = fat32_allocate_cluster(fs);
		if (cluster == 0)
		{
			printk("No space for new file\n");
			return -1;
		}
		printk("New cluster: %d\n", cluster);
		entry->first_cluster_low = cluster & 0xFFFF;
		entry->first_cluster_high = (cluster >> 16) & 0xFFFF;
	}
	printk("entry :");
	for (int i = 0; i < 256; i++)
		printk("%c", entry[i]);
	printk("\n");

	size_t buf_offset = 0;
	size_t remaining = size;

	while (remaining > 0 && cluster < 0x0FFFFFF8)
	{
		uint8_t *cluster_buf = kmalloc(fs->cluster_size, GFP_KERNEL);
		if (!cluster_buf)
			return -1;

		// визначаємо позицію в кластері
		size_t in_cluster_offset = file->pos % fs->cluster_size;
		size_t space_in_cluster = fs->cluster_size - in_cluster_offset;
		size_t to_write = (remaining < space_in_cluster) ? remaining : space_in_cluster;

		// читаємо старі дані, якщо не перезаписуємо весь кластер
		if (to_write < fs->cluster_size)
		{
			fat32_read_cluster(fs, cluster, cluster_buf);
		}
		else
			memset(cluster_buf, 0, fs->cluster_size);

		memcpy(cluster_buf + in_cluster_offset, buffer + buf_offset, to_write);
		fat32_write_cluster(fs, cluster, cluster_buf);

		kfree(cluster_buf);

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
					printk("No space during write\n");
					fat_flush(fs);
					return buf_offset;
				}
				set_fat_entry(fs, cluster, next);
			}
			cluster = next;
		}
	}

	// оновлюємо розмір
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
		printk("Deleting directory: %s\n", path);
		fat32_delete_directory(fs, path);
	}
	else
	{
		printk("Deleting file: %s\n", path);
		fat32_delete_file(fs, path);
	}
	return 0;
}

int fat32_init_from_lba(uint32_t first_lba, FAT32_FS *fs)
{
	printk("Mounting FAT32 at LBA %d\n", first_lba);
	uint8_t sector[512];
	printk("Reading FAT32 signature\n");
	fs->read_sector(fs->device, first_lba, sector);
	printk("FAT32 signature: 0x%X 0x%X\n", sector[510], sector[511]);
	if (!(sector[510] == 0x55 && sector[511] == 0xAA))
	{
		printk("Invalid FAT32 signature: 0x%X 0x%X\n", sector[510], sector[511]);
		return -1;
	}

	FAT32_BPB *bpb = kmalloc(sizeof(FAT32_BPB), GFP_KERNEL);
	if (!bpb)
	{
		printk("Failed to allocate memory for BPB\n");
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
	printk("Total sectors: %d\n", fs->total_sectors);
	printk("Sectors per cluster: %d\n", fs->sectors_per_cluster);
	printk("Cluster size: %d\n", fs->cluster_size);
	printk("Total FAT entries: %d\n", fs->total_fat_entries);
	printk("Bytes per sector: %d\n", fs->bytes_per_sector);
	printk("Reserved sectors: %d\n", fs->reserved_sectors);
	printk("Number of FATs: %d\n", fs->num_fats);
	printk("Sectors per FAT: %d\n", fs->sectors_per_fat);
	printk("Root cluster: %d\n", fs->root_cluster);
	printk("Cluster heap LBA: %d\n", fs->cluster_heap_lba);

	uint32_t fat_size_bytes = bpb->fat_size_32 * bpb->bytes_per_sector;
	printk("FAT size in bytes: %d\n", fat_size_bytes);

	fs->fat_cache = kmalloc(fat_size_bytes, GFP_KERNEL);
	if (!fs->fat_cache)
	{
		kfree(bpb);
		printk("Failed to allocate memory for FAT cache\n");
		return -3;
	}

	for (uint32_t i = 0; i < bpb->fat_size_32; i++)
	{
		fs->read_sector(fs->device, fs->fat_start_lba + i,
				((uint8_t *)(fs->fat_cache) + i * bpb->bytes_per_sector));
	}

	fs->fat_dirty = false;
	kfree(bpb); // звільняємо тільки тут, після використання
	return 0;
}

uint32_t cluster_to_lba(FAT32_FS *fs, uint32_t cluster)
{
	return fs->cluster_heap_lba + (cluster - 2) * fs->sectors_per_cluster;
}

int fat32_update_fat_entry(FAT32_FS *fs, FAT32_File *file)
{
	uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
	if (!buf)
		return -1;

	fat32_read_cluster(fs, file->cluster, buf);
	printk("old data: ");
	for (int i = 0; i < 256; i++)
		printk("%c", buf[i]);
	printk("\n");
	FAT32_DirectoryEntry *entries = (FAT32_DirectoryEntry *)buf;
	printk("entry :");
	for (int i = 0; i < 256; i++)
		printk("%c", file->entry[i]);
	printk("\n");

	memcpy(&entries[file->index], file->entry, sizeof(FAT32_DirectoryEntry));
	printk("new data: ");
	for (int i = 0; i < 256; i++)
		printk("%c", buf[i]);
	printk("\n");
	fat32_write_cluster(fs, file->cluster, buf);
	kfree(buf);
	return 0;
}

void fat32_read_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer)
{
	uint32_t lba = cluster_to_lba(fs, cluster);
	for (uint32_t i = 0; i < fs->sectors_per_cluster; i++)
	{
		printk("lba: %d\n", lba + i);
		fs->read_sector(fs->device, lba + i, buffer + i * fs->bytes_per_sector);
	}
}

void fat32_write_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer)
{
	for (int i = 0; i < 256; i++)
	{
		printk("%c", buffer[i]);
	}
	uint32_t lba = cluster_to_lba(fs, cluster);
	for (uint32_t i = 0; i < fs->sectors_per_cluster; i++)
	{
		fs->write_sector(fs->device, lba + i, buffer + i * fs->bytes_per_sector);
	}

	printk("\nWrote cluster %d\n", cluster);
}

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
		kfree(fs->fat_cache);
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
			break;

		temp_out[j++] = toupper(in[i]);
		i++;
	}

	temp_out[12] = '\0';

	for (i = 0; i < 12; i++)
		out11[i] = temp_out[i];
}

PathParts format_folder_path(const char *in)
{
	printk("Formatting folder path: %s\n", in);
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
			char *name = kmalloc(len + 1, GFP_KERNEL);
			if (!name)
				return result; // or panic

			memcpy(name, in, len);
			name[len] = '\0';

			format_filename_fat(name, result.parts[result.count].sfn);

			result.parts[result.count].lfn = kmalloc(len + 1, GFP_KERNEL);
			memcpy(result.parts[result.count].lfn, name, len);
			result.parts[result.count].lfn[len] = '\0';

			kfree(name);
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
		kfree(pp->parts[i].lfn);
	pp->count = 0;
}

int fat32_create_entry(FAT32_FS *fs, uint32_t cluster, PathPart *pp, bool is_dir)
{
	if (!fs)
	{
		printk("fs is null, %u\n", fs->bytes_per_sector);
		return -EINVAL;
	}
	printk("sfn: %s\n", pp->sfn);
	if (cluster == 0)
	{
		printk("Cluster not found: %s\n", pp->lfn);
		return -EINVAL;
	}
	if (find_directory_entry_cluster(fs, cluster, pp->sfn) != 0)
	{
		printk("Entry already exists: %s\n", pp->lfn);
		return -EEXIST;
	}
	uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
	if (!buf)
	{
		printk("Failed to allocate buffer\n");
		return -ENOMEM;
	}
	fat32_read_cluster(fs, cluster, buf);
	printk("Cluster data: ");
	for (int i = 0; i < 256; i++)
		printk("%c", buf[i]);
	printk("\n\n");

	FAT32_DirectoryEntry *entry = (FAT32_DirectoryEntry *)buf;
	for (int i = 0; i < fs->cluster_size / sizeof(FAT32_DirectoryEntry); i++, entry++)
	{

		if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
		{

			memcpy(entry->name, pp->sfn, 11);

			printk("Entry created and found: %s\n", pp->sfn);

			entry->attr = is_dir ? 0x10 : 0x20;
			uint32_t new_cluster = fat32_allocate_cluster(fs);
			printk("Allocated cluster: %u (high=%04x low=%04x)\n",
			       new_cluster,
			       (new_cluster >> 16) & 0xFFFF,
			       new_cluster & 0xFFFF);

			entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
			entry->first_cluster_low = new_cluster & 0xFFFF;

			entry->file_size = 0;
			fat32_write_cluster(fs, cluster, buf);

			kfree(buf);

			// add entry to directory
			if (is_dir)
				fat32_format_directory_cluster(fs, new_cluster, cluster);
			fat_flush(fs);
			return 0;
		}
	}

	kfree(buf);
	return -ENOSPC; // нема місця в директорії
}

int fat32_delete_entry(FAT32_FS *fs, uint32_t cluster, const char *name)
{
	if (cluster == 0)
	{
		printk("Cluster not found: %s\n", name);
		return -EINVAL;
	}
	if (find_directory_entry_cluster(fs, cluster, name) == 0)
	{
		printk("Entry not found: %s\n", name);
		return -ENOENT;
	}
	uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
	if (!buf)
	{
		printk("Failed to allocate buffer\n");
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
			kfree(buf);
			fat_flush(fs);
			return 0;
		}
	}
	kfree(buf);
	return -ENOENT;
}
