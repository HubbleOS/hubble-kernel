// // elf_loader.c
// #include <stdint.h>
// #include <stddef.h>
// #include <string.h>
// #include <mm/vmm.h>
// #include "fs/vfs/vfs.h"
// #include "mm/kmalloc.h"
// #include "printk.h"

// extern void user_enter(uint64_t entry, uint64_t stack);

// #define ELF_MAGIC 0x464c457fUL // 0x7F 'E' 'L' 'F'

// // ELF64 structs (minimal)
// typedef struct
// {
// 	unsigned char e_ident[16];
// 	uint16_t e_type;
// 	uint16_t e_machine;
// 	uint32_t e_version;
// 	uint64_t e_entry;
// 	uint64_t e_phoff;
// 	uint64_t e_shoff;
// 	uint32_t e_flags;
// 	uint16_t e_ehsize;
// 	uint16_t e_phentsize;
// 	uint16_t e_phnum;
// } __attribute__((packed)) Elf64_Ehdr;

// typedef struct
// {
// 	uint32_t p_type;
// 	uint32_t p_flags;
// 	uint64_t p_offset;
// 	uint64_t p_vaddr;
// 	uint64_t p_paddr;
// 	uint64_t p_filesz;
// 	uint64_t p_memsz;
// 	uint64_t p_align;
// } __attribute__((packed)) Elf64_Phdr;

// enum
// {
// 	PT_NULL = 0,
// 	PT_LOAD = 1
// };

// static inline size_t round_down(size_t x, size_t a) { return x & ~(a - 1); }
// static inline size_t round_up(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }

// #define PAGE_SIZE 4096

// #define USER_STACK_PAGES 4
// #define USER_STACK_TOP 0x40400000ULL // безопасный диапазон юзер-памяти (~64MB)

// int map_user_stack(void)
// {
// 	uint64_t stack_top = USER_STACK_TOP;

// 	for (int i = 0; i < USER_STACK_PAGES; i++)
// 	{
// 		uint64_t va = stack_top - (i + 1) * PAGE_SIZE;
// 		uint64_t phys = vmm_alloc_physical_page();
// 		if (!phys)
// 		{
// 			printk("map_user_stack: failed to alloc phys page\n");
// 			return -1;
// 		}

// 		uint64_t flags = PTE_PRESENT | PTE_USER | PTE_WRITABLE;
// 		if (vmm_map_page(va, phys, flags) != 0)
// 		{
// 			printk("map_user_stack: vmm_map_page failed for VA 0x%lx\n", va);
// 			return -1;
// 		}

// 		// memset через виртуальный адрес
// 		memset((void *)va, 0, PAGE_SIZE);
// 		printk("Mapped stack page: VA 0x%lx -> PHYS 0x%lx\n", va, phys);
// 	}
// 	return 0;
// }

// int load_elf_and_run(const char *path)
// {
// 	VFS_File *f = vfs_open(path, VFS_O_RDONLY);
// 	if (!f)
// 	{
// 		printk("load_elf: cannot open %s\n", path);
// 		return -1;
// 	}

// 	Elf64_Ehdr ehdr;
// 	vfs_lseek(f, 0, SEEK_SET);
// 	if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
// 	{
// 		printk("load_elf: read ehdr failed\n");
// 		return -1;
// 	}

// 	uint32_t magic = *(uint32_t *)(&ehdr.e_ident[0]);
// 	if (magic != ELF_MAGIC)
// 	{
// 		printk("load_elf: bad magic 0x%x\n", magic);
// 		return -1;
// 	}

// 	if (ehdr.e_phoff == 0 || ehdr.e_phnum == 0)
// 	{
// 		printk("load_elf: no program headers\n");
// 		return -1;
// 	}

// 	size_t ph_table_size = ehdr.e_phnum * ehdr.e_phentsize;
// 	Elf64_Phdr *phdrs = (Elf64_Phdr *)kmalloc(ph_table_size);
// 	if (!phdrs)
// 	{
// 		printk("load_elf: kmalloc phdrs failed\n");
// 		return -1;
// 	}

// 	vfs_lseek(f, ehdr.e_phoff, SEEK_SET);
// 	if (vfs_read(f, phdrs, ph_table_size) != ph_table_size)
// 	{
// 		printk("load_elf: read phdrs failed\n");
// 		return -1;
// 	}

// 	// Сдвиг всех сегментов в безопасный диапазон (начиная с 0x40000000)
// 	uint64_t base_va = 0x40000000ULL;

// 	for (int i = 0; i < ehdr.e_phnum; i++)
// 	{
// 		Elf64_Phdr *ph = &phdrs[i];
// 		if (ph->p_type != PT_LOAD)
// 			continue;

// 		uint64_t seg_start = round_down(base_va + ph->p_vaddr, PAGE_SIZE);
// 		uint64_t seg_end = round_up(base_va + ph->p_vaddr + ph->p_memsz, PAGE_SIZE);
// 		size_t pages = (seg_end - seg_start) / PAGE_SIZE;

// 		uint64_t map_flags = PTE_PRESENT | PTE_USER;
// 		if (ph->p_flags & 0x2)
// 			map_flags |= PTE_WRITABLE;
// 		if (!(ph->p_flags & 0x1))
// 			map_flags |= PTE_NX; // если нет PF_X, NX

// 		for (size_t pg = 0; pg < pages; pg++)
// 		{
// 			uint64_t va = seg_start + pg * PAGE_SIZE;
// 			uint64_t phys = vmm_alloc_physical_page();
// 			if (!phys)
// 			{
// 				printk("load_elf: alloc phys failed\n");
// 				return -1;
// 			}

// 			if (vmm_map_page(va, phys, map_flags) != 0)
// 			{
// 				printk("load_elf: vmm_map_page failed for VA 0x%lx\n", va);
// 				return -1;
// 			}

// 			memset((void *)va, 0, PAGE_SIZE);

// 			// копируем данные из файла в сегмент
// 			uint64_t page_file_start = ph->p_offset + pg * PAGE_SIZE;
// 			uint64_t page_file_end = page_file_start + PAGE_SIZE;
// 			if (page_file_start < ph->p_offset + ph->p_filesz)
// 			{
// 				size_t to_copy = ph->p_offset + ph->p_filesz - page_file_start;
// 				if (to_copy > PAGE_SIZE)
// 					to_copy = PAGE_SIZE;
// 				vfs_lseek(f, page_file_start, SEEK_SET);
// 				vfs_read((void *)va, (void *)va, to_copy);
// 			}

// 			printk("Mapped segment page: VA 0x%lx -> PHYS 0x%lx\n", va, phys);
// 		}
// 	}

// 	if (map_user_stack() != 0)
// 	{
// 		printk("Failed to map user stack\n");
// 		return -1;
// 	}

// 	printk("Jumping to entry 0x%lx\n", base_va + ehdr.e_entry);
// 	user_enter(base_va + ehdr.e_entry, USER_STACK_TOP);

// 	return 0;
// }

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mm/vmm.h>
#include "fs/vfs/vfs.h"
#include "mm/kmalloc.h"
#include "printk.h"

extern void user_enter(uint64_t entry, uint64_t stack);

#define ELF_MAGIC 0x464c457fUL // 0x7F 'E' 'L' 'F'

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

enum
{
	PT_NULL = 0,
	PT_LOAD = 1
};

#define PAGE_SIZE 4096
#define USER_STACK_PAGES 4
#define USER_STACK_TOP 0x70000000ULL // безопасный диапазон стека

// Identity map stack
static int map_user_stack(void)
{
	for (int i = 0; i < USER_STACK_PAGES; i++)
	{
		uint64_t va = USER_STACK_TOP - (i + 1) * PAGE_SIZE;
		uint64_t phys = vmm_alloc_physical_page();
		if (!phys)
			return -1;

		if (vmm_map_page(va, phys, PTE_PRESENT | PTE_USER | PTE_WRITABLE) != 0)
			return -1;

		memset((void *)va, 0, PAGE_SIZE);
		printk("Mapped stack page: VA 0x%lx -> PHYS 0x%lx\n", va, phys);
	}
	return 0;
}

int load_elf_and_run(const char *path)
{
	VFS_File *f = vfs_open(path, VFS_O_RDONLY);
	if (!f)
	{
		printk("Cannot open %s\n", path);
		return -1;
	}

	Elf64_Ehdr ehdr;
	vfs_lseek(f, 0, SEEK_SET);
	if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
		return -1;

	if (*(uint32_t *)(&ehdr.e_ident[0]) != ELF_MAGIC)
	{
		printk("Bad ELF magic\n");
		return -1;
	}

	size_t ph_table_size = ehdr.e_phnum * ehdr.e_phentsize;
	Elf64_Phdr *phdrs = (Elf64_Phdr *)kmalloc(ph_table_size);
	if (!phdrs)
		return -1;

	vfs_lseek(f, ehdr.e_phoff, SEEK_SET);
	if (vfs_read(f, phdrs, ph_table_size) != ph_table_size)
		return -1;

	// Загружаем PT_LOAD сегменты
	for (int i = 0; i < ehdr.e_phnum; i++)
	{
		Elf64_Phdr *ph = &phdrs[i];
		if (ph->p_type != PT_LOAD)
			continue;

		uint64_t seg_start = ph->p_vaddr & ~(PAGE_SIZE - 1);
		uint64_t seg_end = ((ph->p_vaddr + ph->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
		size_t pages = (seg_end - seg_start) / PAGE_SIZE;

		uint64_t flags = PTE_PRESENT | PTE_USER;
		if (ph->p_flags & 0x2)
			flags |= PTE_WRITABLE;
		if (!(ph->p_flags & 0x1))
			flags |= PTE_NX;

		for (size_t pg = 0; pg < pages; pg++)
		{
			uint64_t va = seg_start + pg * PAGE_SIZE;
			uint64_t phys = vmm_alloc_physical_page();
			if (!phys)
				return -1;

			if (vmm_map_page(va, phys, flags) != 0)
				return -1;

			memset((void *)va, 0, PAGE_SIZE);

			// копируем данные из файла
			uint64_t seg_offset = (seg_start + pg * PAGE_SIZE) - ph->p_vaddr;
			uint64_t page_file_start = ph->p_offset + seg_offset;

			if (page_file_start < ph->p_offset + ph->p_filesz)
			{
				size_t to_copy = ph->p_offset + ph->p_filesz - page_file_start;
				if (to_copy > PAGE_SIZE)
					to_copy = PAGE_SIZE;

				vfs_lseek(f, page_file_start, SEEK_SET);
				// vfs_read((void *)va, (void *)va, to_copy);
				vfs_read(f, (void *)va, to_copy);
			}

			printk("Mapped segment page: VA 0x%lx -> PHYS 0x%lx\n", va, phys);
		}
	}

	if (map_user_stack() != 0)
	{
		printk("Failed to map stack\n");
		return -1;
	}

	// vfs_close(f);
	// kfree(phdrs);

	printk("Jumping to entry 0x%lx\n", ehdr.e_entry);
	user_enter(ehdr.e_entry, USER_STACK_TOP);

	return 0;
}
