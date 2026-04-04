#include <loader_context.h>
#include <bootinfo/bootinfo.h>
#include "higher_half.h"
#include <asm.h>
#include <stdint.h>
#include <stddef.h>
#include <elf.h>

extern void jump_to_kernel(void *boot_info, void *entry, uint64_t stack);

static uint64_t *alloc_page_table(uint64_t *next_free)
{
	uint64_t *pt = (uint64_t *)(*next_free);
	*next_free += 0x1000;
	for (int i = 0; i < 512; i++)
		pt[i] = 0;
	return pt;
}

static void map_2mb(uint64_t *pml4, uint64_t virt, uint64_t phys,
		    uint64_t *next_free)
{
	uint64_t pml4i = (virt >> 39) & 0x1FF;
	uint64_t pdpti = (virt >> 30) & 0x1FF;
	uint64_t pdi = (virt >> 21) & 0x1FF;

	if (!(pml4[pml4i] & 1))
	{
		uint64_t *p = alloc_page_table(next_free);
		pml4[pml4i] = (uint64_t)p | 0x3;
	}
	uint64_t *pdpt = (uint64_t *)(pml4[pml4i] & ~0xFFFULL);

	if (!(pdpt[pdpti] & 1))
	{
		uint64_t *p = alloc_page_table(next_free);
		pdpt[pdpti] = (uint64_t)p | 0x3;
	}
	uint64_t *pd = (uint64_t *)(pdpt[pdpti] & ~0xFFFULL);

	pd[pdi] = phys | 0x83; /* Present + Write + Huge */
}

static void map_4kb(uint64_t *pml4, uint64_t virt, uint64_t phys,
		    uint64_t *next_free)
{
	uint64_t pml4i = (virt >> 39) & 0x1FF;
	uint64_t pdpti = (virt >> 30) & 0x1FF;
	uint64_t pdi = (virt >> 21) & 0x1FF;
	uint64_t pti = (virt >> 12) & 0x1FF;

	if (!(pml4[pml4i] & 1))
	{
		uint64_t *p = alloc_page_table(next_free);
		pml4[pml4i] = (uint64_t)p | 0x3;
	}
	uint64_t *pdpt = (uint64_t *)(pml4[pml4i] & ~0xFFFULL);

	if (!(pdpt[pdpti] & 1))
	{
		uint64_t *p = alloc_page_table(next_free);
		pdpt[pdpti] = (uint64_t)p | 0x3;
	}
	uint64_t *pd = (uint64_t *)(pdpt[pdpti] & ~0xFFFULL);

	if (!(pd[pdi] & 1))
	{
		uint64_t *p = alloc_page_table(next_free);
		pd[pdi] = (uint64_t)p | 0x3;
	}
	uint64_t *pt = (uint64_t *)(pd[pdi] & ~0xFFFULL);

	pt[pti] = phys | 0x3; /* Present + Write */
}

/* Returns virtual entry point, copies PT_LOAD segments to physical memory,
   zeroes BSS (memsz - filesz). */

static uint64_t load_elf(void *elf_buf)
{
	Elf64_Ehdr *ehdr = elf_buf;

	if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E')
		return 0;

	for (int i = 0; i < ehdr->e_phnum; i++)
	{
		Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)elf_buf + ehdr->e_phoff + i * ehdr->e_phentsize);

		if (ph->p_type != PT_LOAD)
			continue;

		uint64_t phys = ph->p_vaddr - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;
		uint8_t *src = (uint8_t *)elf_buf + ph->p_offset;
		uint8_t *dst = (uint8_t *)phys;

		for (uint64_t b = 0; b < ph->p_filesz; b++)
			dst[b] = src[b];

		/* Zero BSS */
		for (uint64_t b = ph->p_filesz; b < ph->p_memsz; b++)
			dst[b] = 0;
	}

	return ehdr->e_entry;
}

/* Walk PT_LOAD segments and find the highest physical byte the kernel needs.
   Used to calculate how much to map at KERNEL_VIRT_BASE. */
static uint64_t elf_phys_end(void *elf_buf)
{
	Elf64_Ehdr *ehdr = elf_buf;
	uint64_t end = KERNEL_PHYS_BASE;

	for (int i = 0; i < ehdr->e_phnum; i++)
	{
		Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)elf_buf + ehdr->e_phoff + i * ehdr->e_phentsize);

		if (ph->p_type != PT_LOAD)
			continue;

		uint64_t seg_end = (ph->p_vaddr - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) + ph->p_memsz;
		if (seg_end > end)
			end = seg_end;
	}

	return end;
}

void boot_main(loader_context_t *ctx)
{
	uint64_t free_base = ctx->free_phys_base;
	uint64_t free_size = ctx->free_phys_size;
	void *rsdp = ctx->rsdp;
	fb_info_t fb = ctx->framebuffer;

	uint64_t kern_phys_end = elf_phys_end(ctx->elf_buf);
	uint64_t pt_arena = (kern_phys_end + 0x1FFFFF) & ~0x1FFFFFULL;
	uint64_t arena = pt_arena + 2 * 1024 * 1024;

	uint64_t *pml4 = alloc_page_table(&pt_arena);

	/* 1. Identity map 0..4GB */
	for (uint64_t p = 0; p < 0x100000000ULL; p += 0x200000)
		map_2mb(pml4, p, p, &pt_arena);

	/* 2. Kernel: KERNEL_VIRT_BASE -> KERNEL_PHYS_BASE
	      Size derived from ELF segments, not ctx->kernel_size. */
	for (uint64_t off = 0; off < kern_phys_end - KERNEL_PHYS_BASE; off += 0x1000)
		map_4kb(pml4, KERNEL_VIRT_BASE + off, KERNEL_PHYS_BASE + off, &pt_arena);

	/* 3. Direct map 0..4GB at DIRECT_MAP_BASE */
	for (uint64_t p = 0; p < 0x100000000ULL; p += 0x200000)
		map_2mb(pml4, DIRECT_MAP_BASE + p, p, &pt_arena);

	/* 4. Recursive mapping */
	pml4[510] = (uint64_t)pml4 | 0x3;

	/* 5. BootInfo */
	BootInfo *boot_info = (BootInfo *)arena;
	arena += sizeof(BootInfo);

	/* 6. Stack після BootInfo */
	uint64_t stack_phys = (arena + 0x1FFFFF) & ~0x1FFFFFULL;
	arena = stack_phys + 16 * 0x1000;
	uint64_t stack_top_virt = DIRECT_MAP_BASE + stack_phys + 16 * 0x1000;

	boot_info->rsdp = rsdp;
	boot_info->framebuffer.base = fb.base;
	boot_info->framebuffer.width = fb.width;
	boot_info->framebuffer.height = fb.height;
	boot_info->framebuffer.pitch = fb.pitch;
	boot_info->framebuffer.bpp = fb.bpp;
	boot_info->memory_map.heap_start = DIRECT_MAP_BASE + arena;
	boot_info->memory_map.heap_size = free_size - (arena - free_base);
	boot_info->memory_map.pml4_phys = (uint64_t)pml4;

	/* 7. Load ELF into physical memory (identity map still active) */
	uint64_t entry_virt = load_elf(ctx->elf_buf);

	/* 8. Switch page tables */
	set_cr3((uint64_t)pml4);

	/* 9. Jump to kernel */
	uint64_t boot_info_virt = DIRECT_MAP_BASE + (uint64_t)boot_info;

	jump_to_kernel((void *)boot_info_virt,
		       (void *)entry_virt,
		       stack_top_virt);
}
