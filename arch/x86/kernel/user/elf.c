#include "elf.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/slab.h"
#include "mm/kmalloc.h"
#include "fs/vfs/vfs.h"
#include "higher_half.h"
#include "printk.h"
#include <stdint.h>
#include <string.h>

#define ELF_MAGIC 0x464c457fUL
#define PAGE_SIZE 4096

#define PT_LOAD 0x1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define USER_STACK_PAGES 16
#define USER_STACK_TOP 0x70000000ULL

/**
 * @brief Load one PT_LOAD segment from ELF file
 *
 * IMPORTANT: We map pages to user virtual addresses, but access them
 * through kernel higher-half mapping (PHYS_TO_VIRT).
 * This works because bootloader identity-mapped first 4GB.
 */
int elf_load_segment(VFS_File *f, Elf64_Phdr *phdr)
{
	if (phdr->p_type != PT_LOAD)
		return 0;

	uint64_t vaddr = phdr->p_vaddr;
	uint64_t memsz = phdr->p_memsz;
	uint64_t filesz = phdr->p_filesz;
	uint64_t offset = phdr->p_offset;

	if (memsz < filesz)
		return -1;

	uint64_t map_start = vaddr & ~(PAGE_SIZE - 1);
	uint64_t map_end = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	printk("[ELF] Loading segment: vaddr=0x%llx size=0x%llx filesz=0x%llx\n",
	       (unsigned long long)vaddr,
	       (unsigned long long)memsz,
	       (unsigned long long)filesz);

	// --- 1) Allocate and map all pages ---
	for (uint64_t addr = map_start; addr < map_end; addr += PAGE_SIZE)
	{
		if (vmm_is_mapped(addr))
		{
			printk("[ELF] WARNING: 0x%llx already mapped, skipping\n", addr);
			continue; // або unmap і замапити нову
		}

		uint64_t phys = pmm_alloc_page();
		if (!phys)
		{
			printk("[ELF] ERROR: Failed to allocate page for 0x%llx\n", addr);
			return -1;
		}

		uint64_t flags = PTE_PRESENT | PTE_USER;
		if (phdr->p_flags & PF_W)
			flags |= PTE_WRITE;
		if (!(phdr->p_flags & PF_X))
			flags |= PTE_NX;
		vmm_unmap_user_page(addr);
		if (vmm_map_page(addr, phys, flags) < 0)
		{
			printk("[ELF] ERROR: Failed to map 0x%llx -> 0x%llx\n", addr, phys);
			pmm_free_page(phys);
			return -1;
		}
		make_pd_entry_user(phys);
		uint8_t *kptr = PHYS_TO_VIRT_PTR(uint8_t, phys);
		kptr[0] = 0xAA;
		uint8_t val = kptr[0];
		if (val != 0xAA)
		{
			printk("[ELF] ERROR: Failed to map 0x%llx -> 0x%llx\n", addr, phys);
			pmm_free_page(phys);
			return -1;
		}

		memset(PHYS_TO_VIRT_PTR(void, phys), 0, PAGE_SIZE); // memset(PHYS_TO_VIRT(phys), 0, PAGE_SIZE);
	}

	// --- 2) Copy file content ---
	if (filesz > 0)
	{
		size_t remaining = (size_t)filesz;
		uint64_t file_offset = 0;

		// Use temporary buffer for reading
		void *kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL); // void *kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!kbuf)
		{
			printk("[ELF] ERROR: Failed to allocate temp buffer\n");
			return -1;
		}

		while (remaining > 0)
		{
			uint64_t dest_va = vaddr + file_offset;
			uint64_t page_base = dest_va & ~(PAGE_SIZE - 1);
			uint64_t in_page_off = dest_va & (PAGE_SIZE - 1);

			size_t chunk = PAGE_SIZE - in_page_off;
			if (chunk > remaining)
				chunk = remaining;

			// Seek to file position
			if (vfs_lseek(f, offset + file_offset, SEEK_SET) < 0)
			{
				printk("[ELF] ERROR: Failed to seek to offset 0x%llx\n",
				       (unsigned long long)(offset + file_offset));
				kfree(kbuf);
				return -1;
			}

			// Read chunk from file
			size_t got = vfs_read(f, kbuf, chunk);
			if (got != chunk)
			{
				printk("[ELF] ERROR: Read returned %zu, expected %zu\n", got, chunk);
				kfree(kbuf);
				return -1;
			}

			// Get physical address of the user page
			uint64_t phys = vmm_get_phys(page_base);
			if (!phys)
			{
				printk("[ELF] ERROR: Page 0x%llx not mapped!\n",
				       (unsigned long long)page_base);
				kfree(kbuf);
				return -1;
			}

			// Copy data via kernel mapping
			void *kaddr = PHYS_TO_VIRT_PTR(void, phys);
			memcpy((uint8_t *)kaddr + in_page_off, kbuf, chunk);
			dump_page(page_base, 0x20);
			file_offset += chunk;
			remaining -= chunk;
		}

		kfree(kbuf);
	}

	printk("[ELF] Segment loaded successfully\n");
	return 0;
}

int elf_load(const char *path, uint64_t *entry_out)
{
	VFS_File *f = vfs_open(path, VFS_O_RDONLY);
	if (!f)
	{
		printk("[ELF] ERROR: Failed to open %s\n", path);
		return -1;
	}

	Elf64_Ehdr ehdr;
	if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
	{
		printk("[ELF] ERROR: Failed to read ELF header\n");
		vfs_close(f);
		return -1;
	}

	// Verify ELF magic
	uint32_t magic = *(uint32_t *)ehdr.e_ident;
	if (magic != ELF_MAGIC)
	{
		printk("[ELF] ERROR: Invalid ELF magic: 0x%x\n", magic);
		vfs_close(f);
		return -1;
	}

	printk("[ELF] Loading %s (entry=0x%llx, phnum=%u)\n",
	       path, (unsigned long long)ehdr.e_entry, ehdr.e_phnum);

	// Read program headers
	size_t ph_size = (size_t)ehdr.e_phnum * sizeof(Elf64_Phdr);
	Elf64_Phdr *phdrs = kmalloc(ph_size, GFP_KERNEL);
	if (!phdrs)
	{
		printk("[ELF] ERROR: Failed to allocate phdrs\n");
		vfs_close(f);
		return -1;
	}

	if (vfs_lseek(f, ehdr.e_phoff, SEEK_SET) < 0)
	{
		printk("[ELF] ERROR: Failed to seek to phdrs\n");
		kfree(phdrs);
		vfs_close(f);
		return -1;
	}

	if (vfs_read(f, phdrs, ph_size) != (size_t)ph_size)
	{
		printk("[ELF] ERROR: Failed to read phdrs\n");
		kfree(phdrs);
		vfs_close(f);
		return -1;
	}

	// Load all PT_LOAD segments
	for (uint16_t i = 0; i < ehdr.e_phnum; ++i)
	{
		Elf64_Phdr *p = &phdrs[i];
		if (p->p_type == PT_LOAD)
		{
			printk("[ELF] HERE loading PHDR[%u]: vaddr=0x%llx filesz=0x%llx memsz=0x%llx flags=0x%x\n",
			       i,
			       (unsigned long long)p->p_vaddr,
			       (unsigned long long)p->p_filesz,
			       (unsigned long long)p->p_memsz,
			       p->p_flags);

			if (elf_load_segment(f, p) < 0)
			{
				printk("[ELF] ERROR: Failed to load segment %u\n", i);
				kfree(phdrs);
				vfs_close(f);
				return -1;
			}
		}
	}

	kfree(phdrs);
	vfs_close(f);

	*entry_out = ehdr.e_entry;
	printk("[ELF] Load complete, entry=0x%llx\n", (unsigned long long)*entry_out);
	return 0;
}

extern void user_enter(uint64_t entry, uint64_t stack);

int elf_run(uint64_t entry)
{
	printk("[ELF] Setting up user stack at 0x%llx\n",
	       (unsigned long long)USER_STACK_TOP);

	uint64_t stack_base = USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE;

	// Allocate and map stack pages
	for (uint64_t addr = stack_base; addr < USER_STACK_TOP; addr += PAGE_SIZE)
	{
		vmm_unmap_user_page(addr);

		uint64_t phys = pmm_alloc_page();
		// if (vmm_is_mapped(addr))
		// {
		// 	vmm_unmap_user_page(addr);
		// 	pmm_free_page(phys);
		// 	return -1;
		// }
		if (!phys)
		{
			printk("[ELF] Failed to alloc stack page\n");
			return -1;
		}

		if (vmm_map_page(addr, phys, PTE_PRESENT | PTE_USER | PTE_WRITE) < 0)
		{
			printk("[ELF] Failed to map stack page\n");
			pmm_free_page(phys);
			return -1;
		}

		uint8_t *kptr = PHYS_TO_VIRT_PTR(uint8_t, phys);
		kptr[0] = 0xAA;
		uint8_t val = kptr[0];
		if (val != 0xAA)
		{
			printk("[ELF] ERROR: Failed to map stack page\n");
			vmm_unmap_user_page(addr);
			pmm_free_page(phys);
			return -1;
		}

		memset(PHYS_TO_VIRT_PTR(uint8_t, phys), 0, PAGE_SIZE);
	}

	printk("[ELF] Entering userspace at 0x%llx with stack 0x%llx\n",
	       (unsigned long long)entry, (unsigned long long)USER_STACK_TOP);

	// while (1)
	// {
	// 	/* code */
	// };

	printk("address of user_enter: %x\n", VIRT_TO_PHYS(entry));

	printk("[ELF] About to enter userspace:\n");
	printk("  Entry point: 0x%llx\n", (unsigned long long)entry);
	printk("  Stack top: 0x%llx\n", (unsigned long long)USER_STACK_TOP);
	printk("  User CS should be: 0x1B\n");
	printk("  User SS should be: 0x23\n");

	// Verify the entry point is reasonable
	if (entry < 0x400000 || entry > 0x800000)
	{
		printk("[ELF] WARNING: Entry point looks suspicious!\n");
	}
	dump_page(0x400000, 0x20);

	// Verify the entry point mapping
	printk("[ELF] Verifying entry point mapping:\n");
	uint64_t entry_phys = vmm_get_phys(entry);
	printk("  Virtual: 0x%llx\n", (unsigned long long)entry);
	printk("  Physical: 0x%llx\n", (unsigned long long)entry_phys);

	if (!entry_phys)
	{
		printk("[ELF] CRITICAL ERROR: Entry point 0x%llx is NOT mapped!\n", entry);
		return -1;
	}

	// Read via physical address
	uint8_t *phys_ptr = PHYS_TO_VIRT_PTR(uint8_t, entry_phys);
	printk("  Code via phys: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       phys_ptr[0], phys_ptr[1], phys_ptr[2], phys_ptr[3],
	       phys_ptr[4], phys_ptr[5], phys_ptr[6], phys_ptr[7]);

	printk("[ELF] Flushing TLB before userspace entry...\n");
	asm volatile("invlpg (%0)" : : "r"(entry));

	printk("[ELF] Verifying page table chain for 0x%llx:\n", entry);
	uint64_t *pml4 = pml4_table();
	uint64_t pml4e = pml4[PML4_INDEX(entry)];
	printk("  PML4E[%d] = 0x%llx (USER=%d)\n", PML4_INDEX(entry), pml4e, !!(pml4e & PTE_USER));

	uint64_t *pdpt = pdpt_table(entry);
	uint64_t pdpte = pdpt[PDPT_INDEX(entry)];
	printk("  PDPTE[%d] = 0x%llx (USER=%d)\n", PDPT_INDEX(entry), pdpte, !!(pdpte & PTE_USER));

	uint64_t *pd = pd_table(entry);
	uint64_t pde = pd[PD_INDEX(entry)];
	printk("  PDE[%d] = 0x%llx (USER=%d)\n", PD_INDEX(entry), pde, !!(pde & PTE_USER));

	uint64_t *pt = pt_table(entry);
	uint64_t pte = pt[PT_INDEX(entry)];
	printk("  PTE[%d] = 0x%llx (USER=%d)\n", PT_INDEX(entry), pte, !!(pte & PTE_USER));
	// After all ELF segments are loaded, before user_enter():

	user_enter(entry, USER_STACK_TOP);

	// Should never return
	printk("[ELF] ERROR: Returned from userspace!\n");
	return -1;
}

int load_elf_and_run(const char *path)
{
	uint64_t entry;
	if (elf_load(path, &entry) < 0)
		return -1;

	return elf_run(entry);
}
