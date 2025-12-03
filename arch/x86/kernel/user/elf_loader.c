#include "mm/vmm.h"
#include "mm/pmm.h"
#include "elf.h"
#include "fs/vfs/vfs.h"
#include "mm/kmalloc.h"
#include <string.h>
#include <stdint.h>

extern void user_enter(uint64_t entry, uint64_t stack);

#define ELF_MAGIC 0x464c457fUL
#define PAGE_SIZE 4096

#define PT_LOAD 1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define USER_STACK_PAGES 16
#define USER_STACK_TOP 0x70000000ULL

int elf_load(const char *path, uint64_t *entry_out)
{
	VFS_File *f = vfs_open(path, VFS_O_RDONLY);
	if (!f)
		return -1;

	Elf64_Ehdr ehdr;
	if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
	{
		vfs_close(f);
		return -1;
	}

	if (*(uint32_t *)ehdr.e_ident != ELF_MAGIC)
	{
		vfs_close(f);
		return -1;
	}

	Elf64_Phdr *phdrs = kmalloc(sizeof(Elf64_Phdr) * ehdr.e_phnum, GFP_KERNEL);
	if (!phdrs)
	{
		vfs_close(f);
		return -1;
	}

	vfs_lseek(f, ehdr.e_phoff, SEEK_SET);
	vfs_read(f, phdrs, sizeof(Elf64_Phdr) * ehdr.e_phnum);

	// позже тут будет PT_LOAD обработка
	// for (...)

	kfree(phdrs);
	vfs_close(f);

	*entry_out = ehdr.e_entry; // uint64_t, всё ок
	return 0;
}

int elf_run(uint64_t entry)
{
	user_enter(entry, USER_STACK_TOP);
	return 0; // не достигнется
}

int load_elf_and_run(const char *path)
{
	uint64_t entry;
	if (elf_load(path, &entry) < 0)
		return -1;

	return elf_run(entry);
}
