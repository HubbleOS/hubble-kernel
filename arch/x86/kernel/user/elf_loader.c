#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "mm/vmm.h"
#include "fs/vfs/vfs.h"
#include "mm/slab.h"
#include "mm/pmm.h"
#include "printk.h"
#include "higher_half.h"

extern void user_enter(uint64_t entry, uint64_t stack);

#define ELF_MAGIC 0x464c457fUL // "\x7FELF" in little-endian
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

// Load single ELF segment
static int load_segment(VFS_File *f, Elf64_Phdr *phdr)
{
	printk("Loading segment: vaddr=0x%lx filesz=0x%lx memsz=0x%lx\n",
	       phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz);

	uint64_t seg_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
	uint64_t seg_end = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = (seg_end - seg_start) / PAGE_SIZE;

	uint64_t pte_flags = PTE_USER;
	if (phdr->p_flags & PF_W)
		pte_flags |= PTE_WRITE;
	if (!(phdr->p_flags & PF_X))
		pte_flags |= PTE_NX;

	/* Запомним выделенные физ. страницы, чтобы при ошибке откатить */
	uint64_t *allocated_phys = kmalloc(num_pages * sizeof(uint64_t), GFP_KERNEL);
	if (!allocated_phys)
	{
		printk("ERROR: no memory for bookkeeping\n");
		return -1;
	}
	size_t allocated = 0;

	for (size_t pg = 0; pg < num_pages; ++pg)
	{
		uint64_t user_va = seg_start + pg * PAGE_SIZE;

		/* 1) выделяем физическую страницу */
		uint64_t phys = pmm_alloc_page();
		if (!phys)
		{
			printk("ERROR: pmm_alloc_page failed for VA 0x%lx\n", user_va);
			goto fail;
		}

		/* 2) мапим в таблицы */
		if (vmm_map_page(user_va, phys, pte_flags | PTE_PRESENT) != 0)
		{
			printk("ERROR: vmm_map_page failed for VA 0x%lx -> PA 0x%lx\n", user_va, phys);
			pmm_free_page(phys);
			goto fail;
		}

		/* 3) kernel-вид страницы */
		void *kernel_page = (void *)PHYS_TO_VIRT(phys);
		if (!kernel_page)
		{
			printk("ERROR: PHYS_TO_VIRT returned NULL for PA 0x%lx\n", phys);
			vmm_unmap_page(user_va);
			pmm_free_page(phys);
			goto fail;
		}

		/* 4) обнуляем страницу */
		memset(kernel_page, 0, PAGE_SIZE);

		/* 5) читаем данные из ELF, если часть страницы покрыта file data */
		uint64_t page_start = user_va;
		uint64_t page_end = user_va + PAGE_SIZE;
		uint64_t data_start = phdr->p_vaddr;
		uint64_t data_end = phdr->p_vaddr + phdr->p_filesz;

		if (page_end > data_start && page_start < data_end)
		{
			uint64_t copy_start = (page_start > data_start) ? page_start : data_start;
			uint64_t copy_end = (page_end < data_end) ? page_end : data_end;
			size_t copy_size = (size_t)(copy_end - copy_start);
			uint64_t file_offset = phdr->p_offset + (copy_start - data_start);
			uint64_t page_offset = copy_start - page_start;

			vfs_lseek(f, file_offset, SEEK_SET);
			size_t read_bytes = vfs_read(f, (uint8_t *)kernel_page + page_offset, copy_size);
			if (read_bytes != copy_size)
			{
				printk("ERROR: read_bytes %ld != expected %lu\n", read_bytes, copy_size);
				vmm_unmap_page(user_va);
				pmm_free_page(phys);
				goto fail;
			}
		}

		/* Успешно: запомним phys для отката/освобождения позже (если понадобится) */
		allocated_phys[allocated++] = phys;
	}

	/* Всё загружено успешно */
	kfree(allocated_phys);
	return 0;

fail:
	/* откат — unmap + free всех ранее выделенных */
	for (size_t i = 0; i < allocated; ++i)
	{
		uint64_t phys = allocated_phys[i];
		uint64_t virt = seg_start + i * PAGE_SIZE;
		vmm_unmap_page(virt);
		pmm_free_page(phys);
	}
	kfree(allocated_phys);
	return -1;
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

	printk("File %s opened successfully\n", path);

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
	Elf64_Phdr *phdrs = kmalloc(phdrs_size, GFP_KERNEL);
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
