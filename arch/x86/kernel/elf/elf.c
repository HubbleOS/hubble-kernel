#include <stdint.h>
#include "usermode.h"
#include "elf.h"
#include <fs/vfs/vfs.h>
#include <mm/vmm.h>
#include "printk.h"

// Use LOW memory addresses that are mapped by bootloader
#define USER_STACK_SIZE (1024 * 1024) // 1MB stack
#define USER_STACK_TOP 0x40000000     // 1GB - safe in 4GB range

// Helper macros for error pointer handling
#define IS_ERR(ptr) ((uintptr_t)(ptr) >= (uintptr_t)-4095)
#define PTR_ERR(ptr) ((long)(ptr))

void load_and_run_elf(const char *path)
{
	printk("========================================\n");
	printk("Loading ELF: %s\n", path);
	printk("========================================\n");

	VFS_File *file = vfs_open(path, VFS_O_RDONLY);

	if (!file || IS_ERR(file))
	{
		printk("❌ Failed to open %s (error: %ld)\n", path, IS_ERR(file) ? PTR_ERR(file) : -1);
		return;
	}

	printk("✓ File opened successfully\n");
	printk("  File size: %u bytes\n", file->node->size);
	printk("  File position: %u\n", file->pos);

	Elf64_Ehdr ehdr;
	printk("\n--- Reading ELF Header ---\n");
	int bytes_read = vfs_read(file, &ehdr, sizeof(ehdr));

	printk("Read %d bytes (expected %lu)\n", bytes_read, sizeof(ehdr));

	if (bytes_read < (int)sizeof(ehdr))
	{
		printk("❌ Failed to read ELF header\n");

		printk("Raw data read:\n");
		uint8_t *raw = (uint8_t *)&ehdr;
		for (int i = 0; i < bytes_read && i < 64; i++)
		{
			if (i % 16 == 0)
				printk("\n%04x: ", i);
			printk("%02x ", raw[i]);
		}
		printk("\n");

		goto cleanup;
	}

	printk("\nELF Magic: 0x%02x 0x%02x 0x%02x 0x%02x\n",
	       ehdr.e_ident[0], ehdr.e_ident[1],
	       ehdr.e_ident[2], ehdr.e_ident[3]);

	if (ehdr.e_ident[EI_MAG0] != 0x7F ||
	    ehdr.e_ident[EI_MAG1] != 'E' ||
	    ehdr.e_ident[EI_MAG2] != 'L' ||
	    ehdr.e_ident[EI_MAG3] != 'F')
	{
		printk("❌ Not a valid ELF file\n");
		goto cleanup;
	}

	if (ehdr.e_ident[EI_CLASS] != ELFCLASS64)
	{
		printk("❌ Not a 64-bit ELF file (class: %d)\n", ehdr.e_ident[EI_CLASS]);
		goto cleanup;
	}

	printk("✓ Valid 64-bit ELF file\n");
	printk("  Entry point: 0x%lx\n", ehdr.e_entry);
	printk("  Program headers: %d (offset: 0x%lx)\n", ehdr.e_phnum, ehdr.e_phoff);

	// Check if entry point is in mapped range
	if (ehdr.e_entry > 0xFFFFFFFF)
	{
		printk("⚠️  WARNING: Entry point 0x%lx is above 4GB!\n", ehdr.e_entry);
		printk("    Bootloader only maps first 4GB.\n");
		printk("    Recompile with: gcc -Wl,-Ttext-segment=0x10000000\n");
	}

	Elf64_Phdr *phdrs = kmalloc(ehdr.e_phnum * sizeof(Elf64_Phdr));
	if (!phdrs)
	{
		printk("❌ Failed to allocate memory for program headers\n");
		goto cleanup;
	}

	printk("\n--- Reading Program Headers ---\n");
	if (vfs_lseek(file, ehdr.e_phoff, SEEK_SET) != 0)
	{
		printk("❌ Failed to seek to program headers\n");
		goto cleanup_phdrs;
	}

	bytes_read = vfs_read(file, phdrs, ehdr.e_phnum * sizeof(Elf64_Phdr));
	printk("Read %d bytes (expected %lu)\n",
	       bytes_read, ehdr.e_phnum * sizeof(Elf64_Phdr));

	if (bytes_read < (int)(ehdr.e_phnum * sizeof(Elf64_Phdr)))
	{
		printk("❌ Failed to read program headers\n");
		goto cleanup_phdrs;
	}

	printk("✓ Program headers loaded\n");

	// Load segments
	printk("\n--- Loading Segments ---\n");
	for (int i = 0; i < ehdr.e_phnum; i++)
	{
		printk("\nSegment %d: type=0x%x ", i, phdrs[i].p_type);

		if (phdrs[i].p_type == PT_LOAD)
		{
			printk("(PT_LOAD)\n");

			uint64_t vaddr = phdrs[i].p_vaddr;
			uint64_t memsz = phdrs[i].p_memsz;
			uint64_t filesz = phdrs[i].p_filesz;

			printk("  vaddr: 0x%lx, memsz: 0x%lx, filesz: 0x%lx\n", vaddr, memsz, filesz);
			printk("  flags: %c%c%c\n",
			       (phdrs[i].p_flags & PF_R) ? 'R' : '-',
			       (phdrs[i].p_flags & PF_W) ? 'W' : '-',
			       (phdrs[i].p_flags & PF_X) ? 'X' : '-');

			if (vaddr + memsz > 0x100000000ULL)
			{
				printk("  ⚠️  Segment extends beyond 4GB!\n");
			}

			uint64_t pages = (memsz + PAGE_SIZE - 1) / PAGE_SIZE;
			uint64_t start_page = vaddr & ~(PAGE_SIZE - 1);

			printk("  Allocating %lu pages at 0x%lx\n", pages, start_page);

			for (uint64_t p = 0; p < pages; p++)
			{
				uint64_t page_vaddr = start_page + p * PAGE_SIZE;
				uint64_t phys = vmm_alloc_physical_page();

				if (!phys)
				{
					printk("  ❌ Failed to allocate page %lu\n", p);
					goto cleanup_phdrs;
				}

				uint64_t flags = VMM_PRESENT | VMM_USER;
				if (phdrs[i].p_flags & PF_W)
					flags |= VMM_WRITE;
				if (!(phdrs[i].p_flags & PF_X))
					flags |= VMM_NX;

				int ret = vmm_map_page(page_vaddr, phys, flags);
				if (ret != 0)
				{
					printk("  ❌ Failed to map page at 0x%lx\n", page_vaddr);
					goto cleanup_phdrs;
				}
			}

			printk("  ✓ Pages mapped\n");

			if (filesz > 0)
			{
				printk("  Reading data from offset 0x%lx\n", phdrs[i].p_offset);

				if (vfs_lseek(file, phdrs[i].p_offset, SEEK_SET) != 0)
				{
					printk("  ❌ Failed to seek\n");
					goto cleanup_phdrs;
				}

				void *temp_buf = kmalloc(filesz);
				if (!temp_buf)
				{
					printk("  ❌ Failed to allocate buffer\n");
					goto cleanup_phdrs;
				}

				bytes_read = vfs_read(file, temp_buf, filesz);
				printk("  Read %d bytes\n", bytes_read);

				if (bytes_read < (int)filesz)
				{
					printk("  ❌ Short read\n");
					kfree(temp_buf);
					goto cleanup_phdrs;
				}

				memcpy((void *)vaddr, temp_buf, filesz);
				kfree(temp_buf);
				printk("  ✓ Data copied\n");
			}

			if (memsz > filesz)
			{
				printk("  Zeroing BSS (%lu bytes)\n", memsz - filesz);
				memset((void *)(vaddr + filesz), 0, memsz - filesz);
			}
		}
		else
		{
			printk("(skipped)\n");
		}
	}

	// Create user stack
	printk("\n--- Creating User Stack ---\n");
	uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
	uint64_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;

	printk("Stack: 0x%lx - 0x%lx (%lu pages)\n",
	       stack_bottom, USER_STACK_TOP, stack_pages);

	for (uint64_t p = 0; p < stack_pages; p++)
	{
		uint64_t vaddr = stack_bottom + p * PAGE_SIZE;
		uint64_t phys = vmm_alloc_physical_page();

		if (!phys)
		{
			printk("❌ Failed to allocate stack page %lu\n", p);
			goto cleanup_phdrs;
		}

		int ret = vmm_map_page(vaddr, phys,
				       VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NX);
		if (ret != 0)
		{
			printk("❌ Failed to map stack page\n");
			goto cleanup_phdrs;
		}
	}

	printk("✓ Stack created\n");

	kfree(phdrs);

	printk("\n========================================\n");
	printk("✓ ELF loaded successfully!\n");
	printk("  Entry: 0x%lx\n", ehdr.e_entry);
	printk("  Stack: 0x%lx\n", USER_STACK_TOP - 16);
	printk("========================================\n\n");

	// TEMPORARILY DISABLED: Don't jump to usermode yet
	printk("⚠️  Usermode jump DISABLED for testing\n");
	printk("    Uncomment enter_usermode() to test actual execution\n\n");

	/*
	usermode_context_t ctx = {
	    .rip = ehdr.e_entry,
	    .rsp = USER_STACK_TOP - 16,
	    .rflags = 0x202};

	printk("🚀 Jumping to user mode...\n");
	enter_usermode(&ctx);
	*/

	return;

cleanup_phdrs:
	kfree(phdrs);
cleanup:
	printk("\n❌ ELF loading failed\n");
	return;
}
