#include "mm/vmm.h"
#include "mm/pmm.h"
#include "asm.h"
#include "higher_half.h"
#include "elf.h"
#include "fs/vfs/vfs.h"
#include "mm/kmalloc.h"
#include "printk.h"
#include <string.h>

#define USER_STACK_PAGES 16
#define USER_STACK_TOP 0x70000000ULL

extern void user_enter(uint64_t entry, uint64_t stack);

static int load_segment(VFS_File *f, Elf64_Phdr *phdr)
{
	uint64_t seg_start = phdr->p_vaddr & ~(VMM_PAGE_SIZE - 1);
	uint64_t seg_end = (phdr->p_vaddr + phdr->p_memsz + VMM_PAGE_SIZE - 1) & ~(VMM_PAGE_SIZE - 1);
	size_t num_pages = (seg_end - seg_start) / VMM_PAGE_SIZE;

	uint64_t flags = PTE_PRESENT | PTE_USER;
	if (phdr->p_flags & PF_W)
		flags |= PTE_WRITE;
	if (!(phdr->p_flags & PF_X))
		flags |= PTE_NX;

	printk("Loading segment: VA=0x%lx memsz=0x%lx flags=0x%lx\n",
	       phdr->p_vaddr, phdr->p_memsz, flags);

	for (size_t i = 0; i < num_pages; i++)
	{
		uint64_t va = seg_start + i * VMM_PAGE_SIZE;
		uint64_t phys = pmm_alloc_page();
		if (!phys)
		{
			printk("Failed to alloc page for VA 0x%lx\n", va);
			return -1;
		}

		void *kptr = (void *)PHYS_TO_VIRT(phys);
		memset(kptr, 0, VMM_PAGE_SIZE);

		if (vmm_map_page(va, phys, flags) != 0)
		{
			printk("Failed to map VA 0x%lx -> PA 0x%lx\n", va, phys);
			pmm_free_page(phys);
			return -1;
		}

		// copy file data
		uint64_t page_start = va;
		uint64_t page_end = va + VMM_PAGE_SIZE;
		uint64_t data_start = phdr->p_vaddr;
		uint64_t data_end = phdr->p_vaddr + phdr->p_filesz;

		if (page_end > data_start && page_start < data_end)
		{
			uint64_t copy_start = (page_start > data_start) ? page_start : data_start;
			uint64_t copy_end = (page_end < data_end) ? page_end : data_end;
			size_t copy_size = copy_end - copy_start;
			uint64_t file_offset = phdr->p_offset + (copy_start - data_start);
			size_t page_offset = copy_start - page_start;

			vfs_lseek(f, file_offset, SEEK_SET);
			if (vfs_read(f, (uint8_t *)kptr + page_offset, copy_size) != copy_size)
			{
				printk("Failed to read data for VA 0x%lx\n", va);
				return -1;
			}
		}
	}

	return 0;
}

static int map_user_stack(void)
{
	uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_PAGES * VMM_PAGE_SIZE;
	for (int i = 0; i < USER_STACK_PAGES; i++)
	{
		uint64_t va = stack_bottom + i * VMM_PAGE_SIZE;
		uint64_t phys = pmm_alloc_page();
		if (!phys)
		{
			printk("Failed to alloc stack page\n");
			return -1;
		}

		if (vmm_map_page(va, phys, PTE_PRESENT | PTE_USER | PTE_WRITE | PTE_NX) != 0)
		{
			printk("Failed to map stack page\n");
			pmm_free_page(phys);
			return -1;
		}

		memset((void *)PHYS_TO_VIRT(phys), 0, VMM_PAGE_SIZE);
	}

	printk("User stack mapped: 0x%lx - 0x%lx (%d KB)\n",
	       USER_STACK_TOP - USER_STACK_PAGES * VMM_PAGE_SIZE, USER_STACK_TOP, USER_STACK_PAGES * 4);
	return 0;
}

int load_elf_and_run(const char *path)
{
	printk("\n=== Loading ELF: %s ===\n", path);

	VFS_File *f = vfs_open(path, VFS_O_RDONLY);
	if (!f)
	{
		printk("Cannot open %s\n", path);
		return -1;
	}

	Elf64_Ehdr ehdr;
	vfs_lseek(f, 0, SEEK_SET);
	if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
	{
		printk("Failed to read ELF header\n");
		vfs_close(f);
		return -1;
	}

	if (*(uint32_t *)ehdr.e_ident != ELF_MAGIC)
	{
		printk("Invalid ELF magic\n");
		vfs_close(f);
		return -1;
	}

	size_t phdrs_size = ehdr.e_phnum * ehdr.e_phentsize;
	Elf64_Phdr *phdrs = kmalloc(phdrs_size, GFP_KERNEL);
	if (!phdrs)
	{
		printk("Out of memory\n");
		vfs_close(f);
		return -1;
	}

	vfs_lseek(f, ehdr.e_phoff, SEEK_SET);
	if (vfs_read(f, phdrs, phdrs_size) != phdrs_size)
	{
		printk("Failed to read program headers\n");
		kfree(phdrs);
		vfs_close(f);
		return -1;
	}

	for (int i = 0; i < ehdr.e_phnum; i++)
	{
		if (phdrs[i].p_type == PT_LOAD)
		{
			if (load_segment(f, &phdrs[i]) != 0)
			{
				printk("Failed to load segment %d\n", i);
				kfree(phdrs);
				vfs_close(f);
				return -1;
			}
		}
	}

	kfree(phdrs);
	vfs_close(f);

	if (map_user_stack() != 0)
		return -1;

	printk("\n=== Entering user mode ===\n");
	printk("Entry: 0x%lx, Stack top: 0x%lx\n", ehdr.e_entry, USER_STACK_TOP);

	user_enter(ehdr.e_entry, USER_STACK_TOP);
	printk("ERROR: Returned from user mode!\n");
	return -1;
}
