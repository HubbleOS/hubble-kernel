#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "mm/vmm.h"
#include "fs/vfs/vfs.h"
#include "mm/kmalloc.h"
#include "mm/pmm.h"
#include "printk.h"

extern void user_enter(uint64_t entry, uint64_t stack);

#define ELF_MAGIC 0x464c457fUL
#define PAGE_SIZE 4096
#define USER_STACK_PAGES 8
#define USER_STACK_TOP 0x70000000ULL

// ELF structures
typedef struct
{
	unsigned char e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_NULL 0
#define PT_LOAD 1

#define PF_X 0x1 // Executable
#define PF_W 0x2 // Writable
#define PF_R 0x4 // Readable

// Helper: allocate and map user page, return virtual address
static void *alloc_and_map_user_page(uint64_t user_va, uint64_t flags)
{
	// Allocate physical page - pmm_alloc returns VIRTUAL address
	void *phys_virt = pmm_alloc_phys(1);
	if (!phys_virt)
	{
		printk("ERROR: pmm_alloc_phys() returned NULL\n");
		return NULL;
	}

	printk("  pmm_alloc_phys() returned: %p\n", phys_virt);

	// Get physical address
	// uint64_t phys = pmm_get_phys(phys_virt);
	uint64_t phys;
	printk("  Physical address: 0x%lx\n", phys);

	// Map to user space
	printk("  Mapping user VA 0x%lx -> PA 0x%lx, flags 0x%lx\n", user_va, phys, flags);
	if (vmm_map_page(user_va, phys, flags) != 0)
	{
		printk("ERROR: vmm_map_page() failed\n");
		pmm_free_phys(phys_virt, 1);
		return NULL;
	}

	printk("  Mapping successful\n");

	// Return the kernel virtual address for writing
	return phys_virt;
}

// Map and initialize user stack
static int setup_user_stack(void)
{
	printk("Setting up user stack: %d pages at top 0x%lx\n",
	       USER_STACK_PAGES, USER_STACK_TOP);

	for (int i = 0; i < USER_STACK_PAGES; i++)
	{
		uint64_t user_va = USER_STACK_TOP - (i + 1) * PAGE_SIZE;

		void *kernel_virt = alloc_and_map_user_page(
		    user_va,
		    PTE_PRESENT | PTE_USER | PTE_WRITABLE);

		if (!kernel_virt)
		{
			printk("ERROR: Failed to allocate stack page %d\n", i);
			return -1;
		}

		// Zero the page using kernel virtual address
		memset(kernel_virt, 0, PAGE_SIZE);

		printk("  Stack page %d: User VA 0x%lx -> Kernel VA %p\n",
		       i, user_va, kernel_virt);
	}

	return 0;
}

// Load single ELF segment
static int load_segment(VFS_File *f, Elf64_Phdr *phdr)
{
	printk("Loading segment:\n");
	printk("  VAddr:  0x%lx\n", phdr->p_vaddr);
	printk("  Filesz: 0x%lx\n", phdr->p_filesz);
	printk("  Memsz:  0x%lx\n", phdr->p_memsz);
	printk("  Flags:  0x%x (R:%d W:%d X:%d)\n",
	       phdr->p_flags,
	       !!(phdr->p_flags & PF_R),
	       !!(phdr->p_flags & PF_W),
	       !!(phdr->p_flags & PF_X));

	// Calculate page-aligned range
	uint64_t seg_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
	uint64_t seg_end = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = (seg_end - seg_start) / PAGE_SIZE;

	printk("  Pages: %lu (0x%lx - 0x%lx)\n", num_pages, seg_start, seg_end);

	// Determine page flags
	uint64_t flags = PTE_PRESENT | PTE_USER;
	if (phdr->p_flags & PF_W)
	{
		flags |= PTE_WRITABLE;
	}
	if (!(phdr->p_flags & PF_X))
	{
		flags |= PTE_NX;
	}

	// Allocate and map pages
	for (size_t pg = 0; pg < num_pages; pg++)
	{
		uint64_t user_va = seg_start + pg * PAGE_SIZE;

		// Allocate and map - returns kernel virtual address
		void *kernel_page = alloc_and_map_user_page(user_va, flags);

		if (!kernel_page)
		{
			printk("ERROR: Failed to allocate page at VA 0x%lx\n", user_va);
			return -1;
		}

		// Zero the entire page first
		memset(kernel_page, 0, PAGE_SIZE);

		// Calculate what part of this page needs data from file
		uint64_t page_start = user_va;
		uint64_t page_end = user_va + PAGE_SIZE;
		uint64_t data_start = phdr->p_vaddr;
		uint64_t data_end = phdr->p_vaddr + phdr->p_filesz;

		// Check if this page overlaps with file data
		if (page_end > data_start && page_start < data_end)
		{
			// Calculate overlap
			uint64_t copy_start = (page_start > data_start) ? page_start : data_start;
			uint64_t copy_end = (page_end < data_end) ? page_end : data_end;
			size_t copy_size = copy_end - copy_start;

			uint64_t file_offset = phdr->p_offset + (copy_start - data_start);
			uint64_t page_offset = copy_start - page_start;

			// Read directly into kernel page
			vfs_lseek(f, file_offset, SEEK_SET);
			size_t read_bytes = vfs_read(f, (uint8_t *)kernel_page + page_offset, copy_size);

			if (read_bytes != (size_t)copy_size)
			{
				printk("ERROR: Failed to read segment data (got %ld, expected %lu)\n",
				       read_bytes, copy_size);
				return -1;
			}

			printk("    Page 0x%lx: copied 0x%lx bytes at offset 0x%lx\n",
			       user_va, copy_size, page_offset);
		}
		else
		{
			printk("    Page 0x%lx: zero-filled (BSS)\n", user_va);
		}
	}

	return 0;
}

int load_elf_and_run(const char *path)
{
	printk("\n=== Loading ELF: %s ===\n", path);

	// Open file
	VFS_File *f = vfs_open(path, VFS_O_RDONLY);
	if (!f)
	{
		printk("ERROR: Cannot open %s\n", path);
		return -1;
	}

	// Read ELF header
	Elf64_Ehdr ehdr;
	vfs_lseek(f, 0, SEEK_SET);
	if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
	{
		printk("ERROR: Failed to read ELF header\n");
		vfs_close(f);
		return -1;
	}

	// Verify ELF magic
	uint32_t magic = *(uint32_t *)ehdr.e_ident;
	if (magic != ELF_MAGIC)
	{
		printk("ERROR: Invalid ELF magic: 0x%x (expected 0x%x)\n",
		       magic, ELF_MAGIC);
		vfs_close(f);
		return -1;
	}

	printk("ELF Header:\n");
	printk("  Entry point: 0x%lx\n", ehdr.e_entry);
	printk("  Program headers: %d entries at offset 0x%lx\n",
	       ehdr.e_phnum, ehdr.e_phoff);

	// Read program headers
	size_t phdrs_size = ehdr.e_phnum * ehdr.e_phentsize;
	Elf64_Phdr *phdrs = kmalloc(phdrs_size);
	if (!phdrs)
	{
		printk("ERROR: Out of memory for program headers\n");
		vfs_close(f);
		return -1;
	}

	vfs_lseek(f, ehdr.e_phoff, SEEK_SET);
	if (vfs_read(f, phdrs, phdrs_size) != (size_t)phdrs_size)
	{
		printk("ERROR: Failed to read program headers\n");
		kfree(phdrs);
		vfs_close(f);
		return -1;
	}

	// Load all PT_LOAD segments
	printk("\nLoading segments:\n");
	for (int i = 0; i < ehdr.e_phnum; i++)
	{
		if (phdrs[i].p_type == PT_LOAD)
		{
			printk("\n--- Segment %d ---\n", i);
			if (load_segment(f, &phdrs[i]) != 0)
			{
				printk("ERROR: Failed to load segment %d\n", i);
				kfree(phdrs);
				vfs_close(f);
				return -1;
			}
		}
	}

	kfree(phdrs);
	vfs_close(f);

	// Setup user stack
	printk("\n");
	if (setup_user_stack() != 0)
	{
		printk("ERROR: Failed to setup user stack\n");
		return -1;
	}

	// Jump to user mode
	printk("\n=== Entering user mode ===\n");
	printk("Entry point: 0x%lx\n", ehdr.e_entry);
	printk("Stack top:   0x%lx\n", USER_STACK_TOP);
	printk("==============================\n\n");

	user_enter(ehdr.e_entry, USER_STACK_TOP);

	// Should never return
	printk("ERROR: Returned from user mode!\n");
	return -1;
}
