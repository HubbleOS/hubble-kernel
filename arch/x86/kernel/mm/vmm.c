#include "vmm.h"
#include "pmm.h"
#include "asm.h"
#include <string.h>

#include "higher_half.h"

vmm_info_t g_vmm = {0};

// --- Recursive mapping helper ---
#define RECURSIVE_INDEX 510ULL
static inline uint64_t *vmm_get_table(uint64_t virt, int level)
{
	switch (level)
	{
	case 4:
		return (uint64_t *)((RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) | (RECURSIVE_INDEX << 21) | (RECURSIVE_INDEX << 12));
	case 3:
		return (uint64_t *)((RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) | (RECURSIVE_INDEX << 21) | (PML4_INDEX(virt) << 12));
	case 2:
		return (uint64_t *)((RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) | (PDPT_INDEX(virt) << 21) | (PML4_INDEX(virt) << 12));
	case 1:
		return (uint64_t *)((RECURSIVE_INDEX << 39) | (PDPT_INDEX(virt) << 30) | (PD_INDEX(virt) << 21) | (PML4_INDEX(virt) << 12));
	default:
		return NULL;
	}
}

static inline uint64_t pte_addr(uint64_t entry) { return entry & 0x000FFFFFFFFFF000ULL; }
static inline bool pte_present(uint64_t entry) { return entry & PTE_PRESENT; }

static inline uint64_t pte_make(uint64_t phys, uint64_t flags)
{
	uint64_t entry = phys & 0x000FFFFFFFFFF000ULL;
	entry |= (flags & 0xFFF);
	if (flags & PTE_NX)
		entry |= (1ULL << 63);
	return entry;
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

// --- Initialization ---
void vmm_init(void)
{
	g_vmm.pml4_phys = get_cr3();
	g_vmm.pml4_virt = (uint64_t *)PHYS_TO_VIRT(g_vmm.pml4_phys);
	g_vmm.total_mapped_pages = 0;
}

// --- Map page (4KB or huge) ---
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
	size_t idx[4] = {PML4_INDEX(virt), PDPT_INDEX(virt), PD_INDEX(virt), PT_INDEX(virt)};
	uint64_t *tables[4];
	tables[0] = vmm_get_table(virt, 4);

	uint64_t table_flags = PTE_PRESENT | PTE_WRITE;
	if (flags & PTE_USER)
		table_flags |= PTE_USER;

	for (int level = 0; level < 3; level++)
	{
		uint64_t *t = tables[level];
		size_t i = idx[level];
		if (!pte_present(t[i]))
		{
			uint64_t *new_table = vmm_alloc_table();
			if (!new_table)
				return -1;
			t[i] = pte_make(VIRT_TO_PHYS(new_table), table_flags);
		}
		else if ((flags & PTE_USER) && !(t[i] & PTE_USER))
		{
			t[i] |= PTE_USER;
		}
		tables[level + 1] = (uint64_t *)PHYS_TO_VIRT(pte_addr(t[i]));
	}

	uint64_t *pd = tables[2];
	if ((phys % VMM_HUGE_PAGE_SIZE == 0) && (virt % VMM_HUGE_PAGE_SIZE == 0))
	{
		pd[idx[2]] = pte_make(phys, flags | PTE_HUGE);
		g_vmm.total_mapped_pages += VMM_HUGE_PAGE_SIZE / VMM_PAGE_SIZE;
		invlpg((void *)virt);
		return 0;
	}

	uint64_t *pt = tables[3];
	pt[idx[3]] = pte_make(phys, flags);
	g_vmm.total_mapped_pages++;
	invlpg((void *)virt);
	return 0;
}

void vmm_unmap_page(uint64_t virt)
{
	uint64_t *pt = vmm_get_table(virt, 1);
	size_t i = PT_INDEX(virt);
	if (!pte_present(pt[i]))
		return;
	pt[i] = 0;
	g_vmm.total_mapped_pages--;
	invlpg((void *)virt);
}

int vmm_set_flags(uint64_t virt, uint64_t flags)
{
	uint64_t *pt = vmm_get_table(virt, 1);
	size_t i = PT_INDEX(virt);
	if (!pte_present(pt[i]))
		return -1;
	uint64_t phys = pte_addr(pt[i]);
	pt[i] = pte_make(phys, flags);
	invlpg((void *)virt);
	return 0;
}
