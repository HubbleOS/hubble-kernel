#include "utils/gpt/gpt.h"
#include "utils/gpt/gpt_struct.h"
#include "utils/ata/ata.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

uint32_t first_usable_lba = 0;
uint32_t last_usable_lba = 0;

void utf16_to_ascii(uint16_t *src, char *dest, size_t max_chars)
{
	for (size_t i = 0; i < max_chars; i++)
	{
		uint16_t c = src[i];

		if ((c >> 8) == 0 && (c & 0xFF) >= 0x20 && (c & 0xFF) <= 0x7F)
		{
			dest[i] = (char)(c & 0xFF); // OK
		}
		else if ((c & 0xFF) == 0 && (c >> 8) >= 0x20 && (c >> 8) <= 0x7F)
		{
			dest[i] = (char)(c >> 8); // Big endian → swap
		}
		else
		{
			dest[i] = '?'; // Нерозпізнано
		}
	}
	dest[max_chars] = '\0';
}

int gpt_init(gpt_partition_t *partitions)
{
	// GPT_Header gpt_header;
	// printf("sizeof(GPT_Header): %d\n", sizeof(GPT_Header));
	uint8_t buf[512];
	// ata_read_sector(1, buf);
	//     if (partitions->type == 0)
	//     {
	//         //((ATA_Device *)(partitions->device))->read(partitions->device, 1, buf);
	//         if (ata_read_sector(partitions->device, 1, buf) != 0)
	//         {
	//             printf("❌ Failed to read GPT header\n");
	//             int a = ata_read_sector(partitions->device, 1, buf);
	//             printf("ata_read_sector: %d\n", a);
	//             return -1;
	//         }
	//     }
	//     else
	//     {
	//         printf("❌ Unsupported device type\n");
	//         ata_read_sector(partitions->device, 1, buf);
	//     }
	printf("Reading GPT header\n");
	partitions->device->read(partitions->device->device, 1, buf);
	// printf("read address: %p device read: %p\n", partitions->device->read, partitions->device->device);
	GPT_Header *gpt_header = (GPT_Header *)buf;
	printf("GPT Signature: %llx\n", gpt_header->signature);

	if (gpt_header->signature != 0x5452415020494645ULL) // "EFI PART"
	{
		printf("❌ Invalid GPT signature\n");
		printf("GPT Signature: %llx\n", gpt_header->signature);
		return -1;
	}

	printf("GPT valid. Entries: %u\n", gpt_header->num_partition_entries);

	uint32_t total_size = gpt_header->num_partition_entries * gpt_header->sizeof_partition_entry;
	uint8_t *entry_buf = malloc(total_size);
	if (!entry_buf)
	{
		printf("❌ Failed to allocate buffer\n");
		return -1;
	}

	uint32_t sectors_to_read = (total_size + 511) / 512;

	for (uint32_t i = 0; i < sectors_to_read; i++)
	{
		// if (partitions->type == 0)

		partitions->device->read(partitions->device->device, gpt_header->partition_entries_lba + i, entry_buf + (i * 512));

		// ata_read_sector(gpt_header->partition_entries_lba + i, entry_buf + (i * 512));
	}

	for (uint32_t i = 0; i < gpt_header->num_partition_entries; i++)
	{
		GPT_Partition_Entry *entry = (GPT_Partition_Entry *)(entry_buf + i * gpt_header->sizeof_partition_entry);

		int is_empty = 1;
		for (int b = 0; b < 16; b++)
		{
			if (entry->partition_type_guid[b] != 0)
			{
				is_empty = 0;
				break;
			}
		}
		if (is_empty)
			break;
		partitions[i].first_lba = entry->first_lba;
		partitions[i].last_lba = entry->last_lba;

		printf("Partition %d:\n", i);
		printf("  First LBA: %d\n", entry->first_lba);
		printf("  Last LBA: %d\n", entry->last_lba);
		printf("  Attributes: %d\n", entry->attributes);
		first_usable_lba = entry->first_lba;
		last_usable_lba = entry->last_lba;
		printf("lba: %d\n", first_usable_lba);
		char name[37] = {0};
		printf("Raw UTF-16 name bytes:\n");
		for (int j = 0; j < 36; j++)
		{
			printf("%d ", entry->name[j]);
		}
		printf("\n");
		utf16_to_ascii(entry->name, name, 36);
		printf("  Name: %s\n", name);
		memcpy(partitions[i].name, name, 36);
	}

	free(entry_buf);
	printf("GPT initialized\n");
	return 0;
}
