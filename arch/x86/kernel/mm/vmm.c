/**
 * @file vmm.c
 * @brief Virtual Memory Manager implementation (FIXED for userspace)
 */
#include "vmm.h"
#include "pmm.h"
#include "asm.h"
#include <string.h>
#include "higher_half.h"

#include "printk.h"

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

static inline uint64_t pte_make(uint64_t pa, uint64_t flags)
{
	return (pa & 0x000FFFFFFFFFF000ULL) | flags;
}

static uint64_t *vmm_alloc_table(void)
{
	uint64_t phys = pmm_alloc_page();
	if (!phys)
	{
		printk("Failed to allocate page for VMM table\n");
		return NULL;
	}
	uint64_t *virt = PHYS_TO_VIRT_PTR(uint64_t, phys);

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

bool is_mmio(uint64_t pa)
{
	return pa >= 0xFEC00000; // або конкретно 0xFEE00000
}

// --- Map a virtual page to a physical page ---
int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags)
{
	printk("[VMM] Mapping page 0x%llx to 0x%llx\n", va, pa);
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
		{
			printk("[VMM] ERROR: Failed to allocate new PDPT\n");

			return -1;
		}
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
		{
			printk("[VMM] ERROR: Failed to allocate new PD\n");
			return -1;
		}
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
	if ((flags & PTE_USER) == 0 && !is_mmio(pa) &&
	    (pa % VMM_HUGE_PAGE_SIZE == 0) &&
	    (va % VMM_HUGE_PAGE_SIZE == 0))
	{
		pd[PD_INDEX(va)] = pte_make(pa, flags | PTE_HUGE);
		g_vmm.total_mapped_pages += VMM_HUGE_PAGE_SIZE / VMM_PAGE_SIZE;
		invlpg((void *)va);
		printk("[VMM] Mapping huge page 0x%llx to 0x%llx\n", va, pa);
		return 0;
	}

	// For userspace or misaligned addresses, use 4KB pages
	if (!pte_present(pd[PD_INDEX(va)]))
	{
		uint64_t *new_pt = vmm_alloc_table();
		if (!new_pt)
		{
			printk("[VMM] ERROR: Failed to allocate new PT\n");
			return -1;
		}
		pd[PD_INDEX(va)] = pte_make(VIRT_TO_PHYS(new_pt), table_flags);
	}
	else
	{
		// Check if this is a huge page - if so, we can't map 4KB page here
		if (pd[PD_INDEX(va)] & PTE_HUGE)
		{
			// Skip or return error - can't mix huge and 4KB pages
			printk("[VMM] ERROR: Huge page already mapped at 0x%llx\n", va);
			return -1;
		}

		if ((flags & PTE_USER) && !(pd[PD_INDEX(va)] & PTE_USER))
		{
			pd[PD_INDEX(va)] |= PTE_USER;
		}
	}

	uint64_t pte_flags = PTE_PRESENT | PTE_WRITE;

	if (flags & VMM_MAP_NO_CACHE)
		pte_flags |= PTE_PCD | PTE_PWT;

	if ((flags & VMM_MAP_GLOBAL) && !is_mmio(pa))
		pte_flags |= PTE_GLOBAL;

	if (flags & VMM_MAP_USER)
		pte_flags |= PTE_USER;

	uint64_t *pt = pt_table(va);
	pt[PT_INDEX(va)] = pte_make(pa, pte_flags);

	g_vmm.total_mapped_pages++;
	invlpg((void *)va);
	return 0;
}

// --- Unmap a virtual page ---
void vmm_unmap_page(uint64_t va)
{
	uint64_t *pt = pt_table(va);
	if (!pte_present(pt[PT_INDEX(va)]))
	{
		printk("[VMM] ERROR: Page not mapped at 0x%llx\n", va);
		return;
	}
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
bool vmm_is_mapped(uint64_t va)
{
	uint64_t *pt = pt_table(va);
	return pte_present(pt[PT_INDEX(va)]);
}

void vmm_unmap_user_page(uint64_t va)
{
	uint64_t *pd = pd_table(va);
	if (pd[PD_INDEX(va)] & PTE_HUGE)
	{
		// якщо це huge page, просто очистити PD запис
		pd[PD_INDEX(va)] = 0;
		invlpg((void *)va);
		return;
	}

	uint64_t *pt = pt_table(va);
	if (!pte_present(pt[PT_INDEX(va)]))
		return;

	pt[PT_INDEX(va)] = 0;
	g_vmm.total_mapped_pages--;
	invlpg((void *)va);
}

void dump_page_flags(uint64_t va)
{
	uint64_t *pt = pt_table(va);
	uint64_t pte = pt[PT_INDEX(va)];
	printk("VA 0x%llx -> PTE 0x%llx\n", va, pte);

	printk("Flags: PRESENT=%d USER=%d WRITE=%d NX=%d\n",
	       !!(pte & PTE_PRESENT),
	       !!(pte & PTE_USER),
	       !!(pte & PTE_WRITE),
	       !!(pte & PTE_NX));
	// set nx to 0
	// pt[PT_INDEX(va)] = pte & ~PTE_NX;
	// invlpg((void *)va);
}

void dump_page(uint64_t va, size_t len)
{
	uint64_t phys = vmm_get_phys(va);
	if (!phys)
	{
		printk("VA 0x%llx not mapped!\n", va);
		return;
	}
	dump_page_flags(va);
	uint8_t *kptr = PHYS_TO_VIRT_PTR(uint8_t, phys);
	printk("Dumping VA 0x%llx -> PA 0x%llx\n", va, phys);

	for (size_t i = 0; i < len; i++)
	{
		if (i % 16 == 0)
			printk("\n%04zx: ", i);
		printk("%02x ", kptr[i]);
	}
	printk("\n");
}

// Увага: робити лише якщо ти точно знаєш, що вся 2MiB зона безпечна для user.
int make_pd_entry_user(uint64_t va)
{
	uint64_t cr3 = get_cr3();
	uint64_t pml4_idx = (va >> 39) & 0x1FF;
	uint64_t pdpt_idx = (va >> 30) & 0x1FF;
	uint64_t pd_idx = (va >> 21) & 0x1FF;

	uint64_t *pml4 = PHYS_TO_VIRT_PTR(uint64_t, cr3 & ~0xFFFULL);
	uint64_t pml4e = pml4[pml4_idx];
	if (!(pml4e & 1))
		return -1;

	uint64_t *pdpt = PHYS_TO_VIRT_PTR(uint64_t, (pml4e & ~0xFFFULL));
	uint64_t pdpte = pdpt[pdpt_idx];
	if (!(pdpte & 1))
		return -1;

	uint64_t *pd = PHYS_TO_VIRT_PTR(uint64_t, (pdpte & ~0xFFFULL));
	uint64_t pde = pd[pd_idx];

	// Перевіримо чи це large page
	if (!(pde & (1ULL << 7)))
	{
		printk("Not a large page at PDE\n");
		return -1;
	}

	// Додати user біт (біти: bit2 = US)
	uint64_t new_pde = pde | (1ULL << 2);
	pd[pd_idx] = new_pde;

	// Якщо потрібно, очистити NX (bit63) -- залежить від p_flags
	// new_pde &= ~(1ULL<<63);

	// Скинути TLB для цього діапазону
	invlpg((void *)va);
	return 0;
}
void flush_tlb(void)
{
	asm volatile("invlpg (%0)" : : "r"(0) : "memory");
}
