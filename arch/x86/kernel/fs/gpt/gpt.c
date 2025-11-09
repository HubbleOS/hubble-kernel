#include "gpt.h"
#include "gpt_struct.h"
#include "printk.h"
#include <fs/ata/ata.h>
#include <mm/slab.h>

#include <stdint.h>
#include <string.h>

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
	printk("sizeof(GPT_Header): %d\n", sizeof(GPT_Header));
	uint8_t buf[512];

	printk("Reading GPT header\n");
	partitions->device->read(partitions->device->device, 1, buf);
	printk("read address: %p device read: %p\n", partitions->device->read, partitions->device->device);
	GPT_Header *gpt_header = (GPT_Header *)buf;
	printk("GPT Signature: %llx\n", gpt_header->signature);

	if (gpt_header->signature != 0x5452415020494645ULL) // "EFI PART"
	{
		printk("Invalid GPT signature\n");
		printk("GPT Signature: %llx\n", gpt_header->signature);
		return -1;
	}

	printk("GPT valid. Entries: %u\n", gpt_header->num_partition_entries);

	uint32_t total_size = gpt_header->num_partition_entries * gpt_header->sizeof_partition_entry;
	uint8_t *entry_buf = kmalloc(total_size);
	if (!entry_buf)
	{
		printk("Failed to allocate buffer\n");
		return -1;
	}

	uint32_t sectors_to_read = (total_size + 511) / 512;

	for (uint32_t i = 0; i < sectors_to_read; i++)
	{
		// if (partitions->type == 0)

		partitions->device->read(partitions->device->device, gpt_header->partition_entries_lba + i, entry_buf + (i * 512));

		// ata_read_sector(gpt_header->partition_entries_lba + i, entry_buf + (i * 512));
	}
	int8_t partition_count = 0;

	for (uint32_t i = 0; i < gpt_header->num_partition_entries; i++)
	{
		GPT_Partition_Entry *entry = (GPT_Partition_Entry *)(entry_buf + i * gpt_header->sizeof_partition_entry);
		printk("Partition %d:\n", i);
		int is_empty = 1;
		for (int b = 0; b < 16; b++)
		{
			if (entry->partition_type_guid[b] != 0)
			{
				is_empty = 0;
				printk("Partition type GUID: %x\n", entry->partition_type_guid[b]);
				break;
			}
		}
		if (is_empty)
		{
			printk("Partition %d is empty\n", i);
			break;
		}

		partitions[partition_count].first_lba = entry->first_lba;
		partitions[partition_count].last_lba = entry->last_lba;

		printk("Partition %d:\n", i);
		printk("  First LBA: %d\n", entry->first_lba);
		printk("  Last LBA: %d\n", entry->last_lba);
		printk("  Attributes: %d\n", entry->attributes);
		first_usable_lba = entry->first_lba;
		last_usable_lba = entry->last_lba;
		printk("lba: %d\n", first_usable_lba);
		char name[37] = {0};
		printk("Raw UTF-16 name bytes:\n");
		for (int j = 0; j < 36; j++)
		{
			printk("%d ", entry->name[j]);
		}
		printk("\n");
		utf16_to_ascii(entry->name, name, 36);
		printk("  Name: %s\n", name);
		memcpy(partitions[partition_count].name, name, 36);
		partition_count++;
	}

	kfree(entry_buf);
	printk("GPT initialized, %d partitions found\n", partition_count);
	return 0;
}
