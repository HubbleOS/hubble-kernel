#include "gpt.h"
#include "gpt_struct.h"
#include "printk.h"
#include <fs/ata/ata.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include "higher_half.h"

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
	printk("sizeof(GPT_Header): %d\n", sizeof(GPT_Header));
	uint8_t buf[512];

	printk("Reading GPT header\n");

	// CHECK: Make sure device and read function are valid
	if (!partitions || !partitions->device)
	{
		printk("ERROR: Invalid partition structure\n");
		return -1;
	}

	if (!partitions->device->read)
	{
		printk("ERROR: Device read function is NULL\n");
		return -1;
	}

	if (!partitions->device->device)
	{
		printk("ERROR: Device pointer is NULL\n");
		return -1;
	}

	printk("Device: %p, Read: %p\n", partitions->device->device, partitions->device->read);

	partitions->device->read(partitions->device->device, 1, buf);
	printk("read address: %p device read: %p\n", partitions->device->read, partitions->device->device);
	GPT_Header *gpt_header = (GPT_Header *)buf;
	printk("GPT Signature: %llx\n", gpt_header->signature);

	if (gpt_header->signature != 0x5452415020494645ULL)
	{
		printk("Invalid GPT signature\n");
		printk("GPT Signature: %llx\n", gpt_header->signature);
		return -1;
	}

	printk("GPT valid. Entries: %u\n", gpt_header->num_partition_entries);

	uint32_t total_size = gpt_header->num_partition_entries * gpt_header->sizeof_partition_entry;
	uint8_t *entry_buf_phys = (uint8_t *)pmm_alloc_pages((total_size + 0xFFF) / 0x1000);

	if (!entry_buf_phys)
	{
		printk("Failed to allocate physical pages\n");
		return -1;
	}

	uint8_t *entry_buf = PHYS_TO_VIRT_PTR(uint8_t, entry_buf_phys);

	if (!entry_buf)
	{
		printk("Failed to allocate buffer\n");
		return -1;
	}

	uint32_t sectors_to_read = (total_size + 511) / 512;

	for (uint32_t i = 0; i < sectors_to_read; i++)
	{
		partitions->device->read(partitions->device->device, gpt_header->partition_entries_lba + i, entry_buf + (i * 512));
	}

	int8_t partition_count = 0;

	for (uint32_t i = 0; i < gpt_header->num_partition_entries; i++)
	{
		GPT_Partition_Entry *entry = (GPT_Partition_Entry *)(entry_buf + i * gpt_header->sizeof_partition_entry);
		int is_empty = 1;
		for (int b = 0; b < 16; b++)
		{
			if (entry->partition_type_guid[b] != 0)
			{
				is_empty = 0;
				printk("Partition %d type GUID: %x\n", i, entry->partition_type_guid[b]);
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
		printk("  First LBA: %llu\n", entry->first_lba);
		printk("  Last LBA: %llu\n", entry->last_lba);
		printk("  Attributes: 0x%llx\n", entry->attributes);

		first_usable_lba = entry->first_lba;
		last_usable_lba = entry->last_lba;
		printk("lba: %llu\n", first_usable_lba);

		// SAFE: Initialize name buffer
		char *name = kmalloc(37, GFP_KERNEL); // Allocate space for name

		printk("Raw UTF-16 name bytes:\n");
		for (int j = 0; j < 36; j++)
		{
			printk("%04x ", entry->name[j]); // Print as hex words
		}
		printk("\n");

		// CRITICAL: Add bounds checking here
		printk("About to convert UTF-16 name...\n");
		printk("entry->name address: %p\n", entry->name);
		printk("name buffer address: %p\n", name);

		// THIS IS WHERE IT CRASHES - Check if utf16_to_ascii is safe
		utf16_to_ascii(entry->name, name, 36);

		printk("  Name: %s\n", name);
		memcpy(partitions[partition_count].name, name, 36);
		partition_count++;
	}

	// FIX: You're calling kfree on physical address!
	// kfree expects virtual address or you should use pmm_free_pages
	pmm_free_pages((uint64_t)entry_buf_phys, (total_size + 0xFFF) / 0x1000);

	printk("GPT initialized, %d partitions found\n", partition_count);
	return 0;
}
