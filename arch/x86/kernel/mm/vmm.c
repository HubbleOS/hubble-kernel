/**
 * @file vmm.c
 * @brief Virtual Memory Manager implementation (FIXED for userspace)
 */
#include "vmm.h"
#include "pmm.h"
#include "asm.h"
#include <string.h>
#include "higher_half.h"

vmm_info_t g_vmm = {0};

// --- Internal helpers ---
static inline uint64_t pte_addr(uint64_t entry)
{
	return entry & 0x000FFFFFFFFFF000ULL;
}

static inline bool pte_present(uint64_t entry)
{
	return entry & PTE_PRESENT;
}

static inline uint64_t pte_make(uint64_t phys, uint64_t flags)
{
	uint64_t e = phys & 0x000FFFFFFFFFF000ULL;
	e |= (flags & 0xFFF); // Apply all lower 12 bits
	if (flags & PTE_NX)
		e |= (1ULL << 63);
	return e;
}

static uint64_t *vmm_alloc_table(void)
{
	uint64_t phys = pmm_alloc_page();
	if (!phys)
		return NULL;
	uint64_t *virt = (uint64_t *)PHYS_TO_VIRT(phys);
	memset(virt, 0, VMM_PAGE_SIZE);
	return virt;
}

uint64_t vmm_get_phys(uint64_t va)
{
	uint64_t *pt = pt_table(va);
	if (!pte_present(pt[PT_INDEX(va)]))
		return 0;
	return pte_addr(pt[PT_INDEX(va)]);
}

// --- Public API ---
void vmm_init(void)
{
	g_vmm.pml4_phys = get_cr3();
	g_vmm.pml4_virt = pml4_table();
	g_vmm.total_mapped_pages = 0;
}

// --- Map a virtual page to a physical page ---
int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags)
{
	uint64_t *pml4 = pml4_table();

	// CRITICAL: Intermediate tables MUST have USER bit if final page is USER!
	uint64_t table_flags = PTE_PRESENT | PTE_WRITE;
	if (flags & PTE_USER)
		table_flags |= PTE_USER;

	// Allocate PML4 -> PDPT if needed
	if (!pte_present(pml4[PML4_INDEX(va)]))
	{
		uint64_t *new_pdpt = vmm_alloc_table();
		if (!new_pdpt)
			return -1;
		pml4[PML4_INDEX(va)] = pte_make(VIRT_TO_PHYS(new_pdpt), table_flags);
	}
	else
	{
		if ((flags & PTE_USER) && !(pml4[PML4_INDEX(va)] & PTE_USER))
		{
			pml4[PML4_INDEX(va)] |= PTE_USER;
		}
	}

	// Allocate PDPT -> PD if needed
	uint64_t *pdpt = pdpt_table(va);
	if (!pte_present(pdpt[PDPT_INDEX(va)]))
	{
		uint64_t *new_pd = vmm_alloc_table();
		if (!new_pd)
			return -1;
		pdpt[PDPT_INDEX(va)] = pte_make(VIRT_TO_PHYS(new_pd), table_flags);
	}
	else
	{
		if ((flags & PTE_USER) && !(pdpt[PDPT_INDEX(va)] & PTE_USER))
		{
			pdpt[PDPT_INDEX(va)] |= PTE_USER;
		}
	}

	uint64_t *pd = pd_table(va);

	// Use huge pages ONLY for kernel mappings (not userspace)
	if ((flags & PTE_USER) == 0 &&
	    (pa % VMM_HUGE_PAGE_SIZE == 0) &&
	    (va % VMM_HUGE_PAGE_SIZE == 0))
	{
		pd[PD_INDEX(va)] = pte_make(pa, flags | PTE_HUGE);
		g_vmm.total_mapped_pages += VMM_HUGE_PAGE_SIZE / VMM_PAGE_SIZE;
		invlpg((void *)va);
		return 0;
	}

	// For userspace or misaligned addresses, use 4KB pages
	if (!pte_present(pd[PD_INDEX(va)]))
	{
		uint64_t *new_pt = vmm_alloc_table();
		if (!new_pt)
			return -1;
		pd[PD_INDEX(va)] = pte_make(VIRT_TO_PHYS(new_pt), table_flags);
	}
	else
	{
		// Check if this is a huge page - if so, we can't map 4KB page here
		if (pd[PD_INDEX(va)] & PTE_HUGE)
		{
			// Skip or return error - can't mix huge and 4KB pages
			return -1;
		}

		if ((flags & PTE_USER) && !(pd[PD_INDEX(va)] & PTE_USER))
		{
			pd[PD_INDEX(va)] |= PTE_USER;
		}
	}

	// Map normal 4KB page
	uint64_t *pt = pt_table(va);
	pt[PT_INDEX(va)] = pte_make(pa, flags);
	g_vmm.total_mapped_pages++;
	invlpg((void *)va);
	return 0;
}

// --- Unmap a virtual page ---
void vmm_unmap_page(uint64_t va)
{
	uint64_t *pt = pt_table(va);
	if (!pte_present(pt[PT_INDEX(va)]))
		return;
	pt[PT_INDEX(va)] = 0;
	g_vmm.total_mapped_pages--;
	invlpg((void *)va);
}

// --- Change flags of an existing page ---
int vmm_set_flags(uint64_t va, uint64_t flags)
{
	uint64_t *pt = pt_table(va);
	if (!pte_present(pt[PT_INDEX(va)]))
		return -1;
	uint64_t pa = pte_addr(pt[PT_INDEX(va)]);
	pt[PT_INDEX(va)] = pte_make(pa, flags);
	invlpg((void *)va);
	return 0;
}
