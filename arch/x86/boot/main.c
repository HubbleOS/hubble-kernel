#include <loader_context.h>
#include <bootinfo/bootinfo.h>
#include "higher_half.h"
#include <asm.h>
#include <stdint.h>
#include <stddef.h>

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

	pd[pdi] = phys | 0x83; // Present + Write + Huge
}

void boot_main(loader_context_t *ctx)
{
	uint64_t free_base = ctx->free_phys_base;
	uint64_t free_size = ctx->free_phys_size;
	void *rsdp = ctx->rsdp;
	uint64_t kern_phys = ctx->kernel_phys;
	fb_info_t fb = ctx->framebuffer;

	// Arena for page tables (2MB)
	uint64_t pt_arena = free_base;
	uint64_t arena = free_base + 2 * 1024 * 1024;

	uint64_t *pml4 = alloc_page_table(&pt_arena);

	// 1. Identity map 0..4GB
	for (uint64_t p = 0; p < 0x100000000ULL; p += 0x200000)
		map_2mb(pml4, p, p, &pt_arena);

	// 2. Higher-half 0..2GB (without overflow)
	for (uint64_t p = 0; p < 0x80000000ULL; p += 0x200000)
		map_2mb(pml4, PHYS_TO_VIRT(p), p, &pt_arena);

	// 3. Recursive mapping
	pml4[510] = (uint64_t)pml4 | 0x3;

	// Stack (16 pages)
	uint64_t stack_phys = arena;
	arena += 16 * 0x1000;
	uint64_t stack_top_virt = PHYS_TO_VIRT(stack_phys + 16 * 0x1000);

	// BootInfo
	BootInfo *boot_info = (BootInfo *)arena;
	arena += 0x1000;

	boot_info->rsdp = rsdp;
	boot_info->framebuffer_data = *(framebuffer_info_t *)&fb;
	boot_info->framebuffer = &boot_info->framebuffer_data;

	boot_info->memory_data.heap_start = arena;
	boot_info->memory_data.heap_size = free_size - (arena - free_base);
	boot_info->memory_data.pml4_phys = (uint64_t)pml4;
	boot_info->memory_map = &boot_info->memory_data;

	set_cr3((uint64_t)pml4);

	uint64_t boot_info_virt = PHYS_TO_VIRT((uint64_t)boot_info);
	uint64_t kernel_virt_entry = PHYS_TO_VIRT(kern_phys);

	jump_to_kernel((void *)boot_info_virt,
		       (void *)kernel_virt_entry,
		       stack_top_virt);
}
