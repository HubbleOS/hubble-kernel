/**
 * @file vmm.c
 * @brief Virtual Memory Manager - Optimized Higher-Half Implementation
 */

#include "vmm.h"
#include "pmm.h"
#include "higher_half.h"
#include "asm.h"
#include <string.h>
#include <stddef.h>

// ============================================================================
// Global State
// ============================================================================

// static vmm_info_t g_vmm = {0};
vmm_info_t g_vmm = {0};

static uint64_t g_kernel_heap_next = 0;

// ============================================================================
// Recursive Mapping Helpers - Optimized
// ============================================================================

// Recursive mapping base для PML4[510]

/**
 * @brief Швидкий доступ до page table entries через recursive mapping
 */
// Рекурсивный индекс PML4
#define RECURSIVE_INDEX 510ULL

static inline uint64_t *vmm_get_pml4(void)
{
	// PML4 через рекурсивное отображение
	return (uint64_t *)((RECURSIVE_INDEX << 39) |
			    (RECURSIVE_INDEX << 30) |
			    (RECURSIVE_INDEX << 21) |
			    (RECURSIVE_INDEX << 12));
}

static inline uint64_t *vmm_get_pdpt(uint64_t virt)
{
	size_t pml4_idx = PML4_INDEX(virt);

	return (uint64_t *)((RECURSIVE_INDEX << 39) |
			    (RECURSIVE_INDEX << 30) |
			    (RECURSIVE_INDEX << 21) |
			    (pml4_idx << 12));
}

static inline uint64_t *vmm_get_pd(uint64_t virt)
{
	size_t pml4_idx = PML4_INDEX(virt);
	size_t pdpt_idx = PDPT_INDEX(virt);

	return (uint64_t *)((RECURSIVE_INDEX << 39) |
			    (RECURSIVE_INDEX << 30) |
			    (pml4_idx << 21) |
			    (pdpt_idx << 12));
}

static inline uint64_t *vmm_get_pt(uint64_t virt)
{
	size_t pml4_idx = PML4_INDEX(virt);
	size_t pdpt_idx = PDPT_INDEX(virt);
	size_t pd_idx = PD_INDEX(virt);

	return (uint64_t *)((RECURSIVE_INDEX << 39) |
			    (pml4_idx << 30) |
			    (pdpt_idx << 21) |
			    (pd_idx << 12));
}

// ============================================================================
// Page Table Entry Helpers
// ============================================================================

static inline uint64_t pte_addr(uint64_t entry)
{
	return entry & 0x000FFFFFFFFFF000ULL;
}

static inline bool pte_present(uint64_t entry)
{
	return entry & PTE_PRESENT;
}

// static inline uint64_t pte_make(uint64_t addr, uint64_t flags)
// {
// 	return (addr & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFF) | PTE_PRESENT;
// }

static inline uint64_t pte_make(uint64_t phys, uint64_t flags)
{
	// Физический адрес должен быть выровнен на 4KB
	phys &= 0x000FFFFFFFFFF000ULL;

	uint64_t entry = phys | (flags & 0xFFF) | PTE_PRESENT;

	if (flags & PTE_NX)
		entry |= (1ULL << 63);

	if (flags & PTE_HUGE)
		entry |= PTE_HUGE;

	return entry;
}

// ============================================================================
// Page Table Allocation
// ============================================================================

/**
 * @brief Виділити та очистити нову page table
 */
static uint64_t *vmm_alloc_table(void)
{
	uint64_t phys = pmm_alloc_page();
	if (phys == 0)
		return NULL;

	uint64_t *table = (uint64_t *)PHYS_TO_VIRT(phys);
	memset(table, 0, PAGE_SIZE);

	return table;
}

/**
 * @brief Отримати або створити table entry
 */
static int vmm_ensure_table(uint64_t *table, size_t idx)
{
	if (pte_present(table[idx]))
		return 0;

	uint64_t *new_table = vmm_alloc_table();
	if (!new_table)
		return -1;

	uint64_t phys = VIRT_TO_PHYS(new_table);
	table[idx] = pte_make(phys, PTE_PRESENT | PTE_WRITE | PTE_USER);

	return 0;
}

// ============================================================================
// Core Mapping Functions - Optimized with Recursive Mapping
// ============================================================================

void vmm_init(void)
{
	g_vmm.pml4_phys = get_cr3();
	g_vmm.pml4_virt = (uint64_t *)PHYS_TO_VIRT(g_vmm.pml4_phys);

	// ✅ Перевірка recursive mapping
	printk("VMM Init: PML4 phys=0x%lx virt=0x%lx\n", g_vmm.pml4_phys, (uint64_t)g_vmm.pml4_virt);
	printk("Checking PML4[510] = 0x%lx\n", g_vmm.pml4_virt[510]);
	printk("Checking PML4[511] = 0x%lx\n", g_vmm.pml4_virt[511]);

	g_kernel_heap_next = HEAP_VIRT_START;
	g_vmm.total_mapped_pages = 0;
	g_vmm.kernel_pages = 0;
}

// Замените vmm_map_page() на эту версию БЕЗ лишних printk:

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
	phys = PAGE_ALIGN_DOWN(phys);

	size_t pml4_idx = PML4_INDEX(virt);
	size_t pdpt_idx = PDPT_INDEX(virt);
	size_t pd_idx = PD_INDEX(virt);
	size_t pt_idx = PT_INDEX(virt);

	uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(g_vmm.pml4_phys);

	// Промежуточные таблицы должны иметь USER-бит, если страница USER
	uint64_t table_flags = PTE_PRESENT | PTE_WRITE;
	if (flags & PTE_USER)
		table_flags |= PTE_USER;

	// --- PML4 ---
	if (!pte_present(pml4[pml4_idx]))
	{
		uint64_t *new_pdpt = vmm_alloc_table();
		if (!new_pdpt)
			return -1;
		pml4[pml4_idx] = pte_make(VIRT_TO_PHYS(new_pdpt), table_flags);
	}
	else if ((flags & PTE_USER) && !(pml4[pml4_idx] & PTE_USER))
	{
		pml4[pml4_idx] |= PTE_USER;
	}

	uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pte_addr(pml4[pml4_idx]));

	// --- PDPT ---
	if (!pte_present(pdpt[pdpt_idx]))
	{
		uint64_t *new_pd = vmm_alloc_table();
		if (!new_pd)
			return -1;
		pdpt[pdpt_idx] = pte_make(VIRT_TO_PHYS(new_pd), table_flags);
	}
	else if ((flags & PTE_USER) && !(pdpt[pdpt_idx] & PTE_USER))
	{
		pdpt[pdpt_idx] |= PTE_USER;
	}

	uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pte_addr(pdpt[pdpt_idx]));

	// --- PD ---
	if (!pte_present(pd[pd_idx]))
	{
		uint64_t *new_pt = vmm_alloc_table();
		if (!new_pt)
			return -1;
		pd[pd_idx] = pte_make(VIRT_TO_PHYS(new_pt), table_flags);
	}
	else if ((flags & PTE_USER) && !(pd[pd_idx] & PTE_USER))
	{
		pd[pd_idx] |= PTE_USER;
	}

	uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pte_addr(pd[pd_idx]));

	// --- Финальная страница ---
	pt[pt_idx] = pte_make(phys, flags | PTE_PRESENT);

	invlpg((void *)virt);

	g_vmm.total_mapped_pages++;
	return 0;
}

void vmm_unmap_page(uint64_t virt)
{
	uint64_t *pt = vmm_get_pt(virt);
	size_t pt_idx = PT_INDEX(virt);

	if (!pte_present(pt[pt_idx]))
		return;

	pt[pt_idx] = 0;
	g_vmm.total_mapped_pages--;

	invlpg((void *)virt);
}

uint64_t vmm_get_physical(uint64_t virt)
{
	// Получаем индексы
	size_t pml4_idx = PML4_INDEX(virt);
	size_t pdpt_idx = PDPT_INDEX(virt);
	size_t pd_idx = PD_INDEX(virt);
	size_t pt_idx = PT_INDEX(virt);

	// Получаем PML4 напрямую
	uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(g_vmm.pml4_phys);

	// Проверяем PML4
	if (!pte_present(pml4[pml4_idx]))
	{
		return 0;
	}

	// Получаем PDPT
	uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pte_addr(pml4[pml4_idx]));
	if (!pte_present(pdpt[pdpt_idx]))
	{
		return 0;
	}

	// ✅ Проверяем на 1GB huge page в PDPT
	if (pdpt[pdpt_idx] & PTE_HUGE)
	{
		// 1GB huge page
		uint64_t phys_base = pdpt[pdpt_idx] & ~0x3FFFFFFFULL;
		uint64_t offset = virt & 0x3FFFFFFFULL;
		return phys_base + offset;
	}

	// Получаем PD
	uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pte_addr(pdpt[pdpt_idx]));
	if (!pte_present(pd[pd_idx]))
	{
		return 0;
	}

	// ✅ Проверяем на 2MB huge page в PD
	if (pd[pd_idx] & PTE_HUGE)
	{
		// 2MB huge page
		uint64_t phys_base = pd[pd_idx] & ~0x1FFFFFULL;
		uint64_t offset = virt & 0x1FFFFFULL;
		return phys_base + offset;
	}

	// Получаем PT (только если не huge page)
	uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pte_addr(pd[pd_idx]));
	if (!pte_present(pt[pt_idx]))
	{
		return 0;
	}

	// Обычная 4KB страница
	uint64_t phys_base = pte_addr(pt[pt_idx]);
	uint64_t offset = PAGE_OFFSET(virt);

	return phys_base + offset;
}

int vmm_set_flags(uint64_t virt, uint64_t flags)
{
	uint64_t *pt = vmm_get_pt(virt);
	size_t pt_idx = PT_INDEX(virt);

	if (!pte_present(pt[pt_idx]))
		return -1;

	uint64_t phys = pte_addr(pt[pt_idx]);
	pt[pt_idx] = pte_make(phys, flags);

	invlpg((void *)virt);

	return 0;
}
