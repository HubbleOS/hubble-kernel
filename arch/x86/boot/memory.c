#include "memory.h"
#include "higher_half.h"
#include <stddef.h>

#define MIN_HEAP_SIZE (64 * 1024 * 1024)

// ============================================================
// Page table helpers
// ============================================================

static EFI_STATUS alloc_page(EFI_BOOT_SERVICES *bs, EFI_PHYSICAL_ADDRESS *addr)
{
	EFI_STATUS status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, addr);
	if (EFI_ERROR(status))
		return status;

	UINT64 *page = (UINT64 *)*addr;
	for (int i = 0; i < 512; i++)
		page[i] = 0;

	return EFI_SUCCESS;
}

static EFI_STATUS map_2mb(EFI_BOOT_SERVICES *bs, UINT64 *pml4, UINT64 virt, UINT64 phys)
{
	UINT64 pml4_idx = (virt >> 39) & 0x1FF;
	UINT64 pdpt_idx = (virt >> 30) & 0x1FF;
	UINT64 pd_idx = (virt >> 21) & 0x1FF;

	if (!(pml4[pml4_idx] & 0x1))
	{
		EFI_PHYSICAL_ADDRESS pdpt_addr = 0;
		EFI_STATUS status = alloc_page(bs, &pdpt_addr);
		if (EFI_ERROR(status))
			return status;
		pml4[pml4_idx] = pdpt_addr | 0x3;
	}

	UINT64 *pdpt = (UINT64 *)(pml4[pml4_idx] & ~0xFFFULL);

	if (!(pdpt[pdpt_idx] & 0x1))
	{
		EFI_PHYSICAL_ADDRESS pd_addr = 0;
		EFI_STATUS status = alloc_page(bs, &pd_addr);
		if (EFI_ERROR(status))
			return status;
		pdpt[pdpt_idx] = pd_addr | 0x3;
	}

	UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFULL);
	pd[pd_idx] = phys | 0x83; // Present + Write + Huge (2MB)

	return EFI_SUCCESS;
}

// ============================================================
// Build page tables
// ============================================================

static EFI_STATUS build_page_tables(EFI_BOOT_SERVICES *bs, EFI_PHYSICAL_ADDRESS *pml4_out)
{
	EFI_STATUS status;
	EFI_PHYSICAL_ADDRESS pml4_addr = 0;

	status = alloc_page(bs, &pml4_addr);
	if (EFI_ERROR(status))
		return status;

	UINT64 *pml4 = (UINT64 *)pml4_addr;

	// 1. Identity map 0-4GB
	for (UINT64 phys = 0; phys < 0x100000000ULL; phys += 0x200000)
	{
		status = map_2mb(bs, pml4, phys, phys);
		if (EFI_ERROR(status))
			return status;
	}

	// 2. Higher-half mapping: 0xFFFFFFFF80000000 -> 0x0
	EFI_PHYSICAL_ADDRESS hh_pdpt_addr = 0;
	status = alloc_page(bs, &hh_pdpt_addr);
	if (EFI_ERROR(status))
		return status;

	pml4[511] = hh_pdpt_addr | 0x3;

	UINT64 *hh_pdpt = (UINT64 *)hh_pdpt_addr;

	for (UINT64 offset = 0; offset < 0x100000000ULL; offset += 0x200000)
	{
		UINT64 virt = PHYS_TO_VIRT(offset);
		UINT64 pdpt_idx = (virt >> 30) & 0x1FF;
		UINT64 pd_idx = (virt >> 21) & 0x1FF;

		if (!(hh_pdpt[pdpt_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pd_addr = 0;
			status = alloc_page(bs, &pd_addr);
			if (EFI_ERROR(status))
				return status;
			hh_pdpt[pdpt_idx] = pd_addr | 0x3;
		}

		UINT64 *pd = (UINT64 *)(hh_pdpt[pdpt_idx] & ~0xFFFULL);
		pd[pd_idx] = offset | 0x83;
	}

	// 3. Recursive mapping в PML4[510]
	pml4[510] = pml4_addr | 0x3;

	*pml4_out = pml4_addr;
	return EFI_SUCCESS;
}

// ============================================================
// Find largest conventional memory region
// ============================================================

static EFI_STATUS find_largest_region(EFI_BOOT_SERVICES *bs,
				      EFI_PHYSICAL_ADDRESS *heap_phys,
				      UINT64 *heap_size)
{
	EFI_STATUS status;
	EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
	UINTN map_size = 0, map_key, desc_size;
	UINT32 desc_version;

	status = bs->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
	if (status != EFI_BUFFER_TOO_SMALL)
		return status;

	map_size += desc_size * 10;
	status = bs->AllocatePool(EfiLoaderData, map_size, (void **)&mem_map);
	if (EFI_ERROR(status))
		return status;

	status = bs->GetMemoryMap(&map_size, mem_map, &map_key, &desc_size, &desc_version);
	if (EFI_ERROR(status))
	{
		bs->FreePool(mem_map);
		return status;
	}

	UINTN count = map_size / desc_size;
	EFI_MEMORY_DESCRIPTOR *largest = NULL;
	UINT64 largest_size = 0;

	for (UINTN i = 0; i < count; i++)
	{
		EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mem_map + i * desc_size);

		if (desc->Type == EfiConventionalMemory)
		{
			UINT64 size = desc->NumberOfPages * EFI_PAGE_SIZE;
			if (size > largest_size)
			{
				largest_size = size;
				largest = desc;
			}
		}
	}

	if (!largest || largest_size < MIN_HEAP_SIZE)
	{
		bs->FreePool(mem_map);
		return EFI_OUT_OF_RESOURCES;
	}

	*heap_phys = largest->PhysicalStart;
	*heap_size = largest_size;

	bs->FreePool(mem_map);
	return EFI_SUCCESS;
}

// ============================================================
// Public API
// ============================================================

EFI_STATUS boot_memory_init(EFI_BOOT_SERVICES *bs, boot_memory_t *out)
{
	EFI_STATUS status;

	// Найти largest region
	EFI_PHYSICAL_ADDRESS heap_phys = 0;
	UINT64 heap_size = 0;

	status = find_largest_region(bs, &heap_phys, &heap_size);
	if (EFI_ERROR(status))
		return status;

	// Зарезервировать heap
	UINTN heap_pages = heap_size / EFI_PAGE_SIZE;
	status = bs->AllocatePages(AllocateAddress, EfiLoaderData, heap_pages, &heap_phys);
	if (EFI_ERROR(status))
		return status;

	// Построить page tables
	EFI_PHYSICAL_ADDRESS pml4_phys = 0;
	status = build_page_tables(bs, &pml4_phys);
	if (EFI_ERROR(status))
		return status;

	out->pml4_phys = pml4_phys;
	out->heap_phys = heap_phys;
	out->heap_size = heap_size;

	return EFI_SUCCESS;
}
