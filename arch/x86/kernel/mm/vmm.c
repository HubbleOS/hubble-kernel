/**
 * @file vmm.c
 * @brief Virtual Memory Manager - Optimized Higher-Half Implementation
 */

#include "vmm.h"
#include "pmm.h"
#include "higher_half.h"
#include <string.h>
#include <stddef.h>

// ============================================================================
// Global State
// ============================================================================

static vmm_info_t g_vmm = {0};
static uint64_t g_kernel_heap_next = 0;

// ============================================================================
// Recursive Mapping Helpers - Optimized
// ============================================================================

// Recursive mapping base для PML4[510]
#define RECURSIVE_BASE 0xFFFFFD8000000000ULL

/**
 * @brief Швидкий доступ до page table entries через recursive mapping
 */
static inline uint64_t *vmm_get_pml4(void)
{
	return (uint64_t *)(RECURSIVE_BASE | (510ULL << 39) | (510ULL << 30) | (510ULL << 21) | (510ULL << 12));
}

static inline uint64_t *vmm_get_pdpt(uint64_t virt)
{
	uint64_t pml4_idx = PML4_INDEX(virt);
	return (uint64_t *)(RECURSIVE_BASE | (510ULL << 39) | (510ULL << 30) | (510ULL << 21) | (pml4_idx << 12));
}

static inline uint64_t *vmm_get_pd(uint64_t virt)
{
	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	return (uint64_t *)(RECURSIVE_BASE | (510ULL << 39) | (510ULL << 30) | (pml4_idx << 21) | (pdpt_idx << 12));
}

static inline uint64_t *vmm_get_pt(uint64_t virt)
{
	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	uint64_t pd_idx = PD_INDEX(virt);
	return (uint64_t *)(RECURSIVE_BASE | (510ULL << 39) | (pml4_idx << 30) | (pdpt_idx << 21) | (pd_idx << 12));
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

static inline uint64_t pte_make(uint64_t addr, uint64_t flags)
{
	return (addr & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFF) | PTE_PRESENT;
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
	table[idx] = pte_make(phys, PTE_WRITE | PTE_USER);

	return 0;
}

// ============================================================================
// Core Mapping Functions - Optimized
// ============================================================================

void vmm_init(void)
{
	g_vmm.pml4_phys = get_cr3();
	g_vmm.pml4_virt = (uint64_t *)PHYS_TO_VIRT(g_vmm.pml4_phys);
	g_kernel_heap_next = HEAP_VIRT_START;

	// Підрахунок існуючих mappings (опціонально)
	g_vmm.total_mapped_pages = 0;
	g_vmm.kernel_pages = 0;
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
	// Перевірка вирівнювання
	if ((virt & 0xFFF) || (phys & 0xFFF))
		return -1;

	// Індекси
	size_t pml4_idx = PML4_INDEX(virt);
	size_t pdpt_idx = PDPT_INDEX(virt);
	size_t pd_idx = PD_INDEX(virt);
	size_t pt_idx = PT_INDEX(virt);

	// Отримуємо PML4
	uint64_t *pml4 = vmm_get_pml4();

	// Забезпечуємо існування PDPT
	if (vmm_ensure_table(pml4, pml4_idx) != 0)
		return -1;

	// Отримуємо PDPT
	uint64_t *pdpt = vmm_get_pdpt(virt);

	// Забезпечуємо існування PD
	if (vmm_ensure_table(pdpt, pdpt_idx) != 0)
		return -1;

	// Отримуємо PD
	uint64_t *pd = vmm_get_pd(virt);

	// Забезпечуємо існування PT
	if (vmm_ensure_table(pd, pd_idx) != 0)
		return -1;

	// Отримуємо PT та створюємо mapping
	uint64_t *pt = vmm_get_pt(virt);

	// Оновлюємо entry
	bool was_present = pte_present(pt[pt_idx]);
	pt[pt_idx] = pte_make(phys, flags);

	if (!was_present)
		g_vmm.total_mapped_pages++;

	// Flush TLB
	invlpg((void *)virt);

	return 0;
}

int vmm_map_range(uint64_t virt, uint64_t phys, size_t size, uint64_t flags)
{
	uint64_t v = PAGE_ALIGN_DOWN(virt);
	uint64_t p = PAGE_ALIGN_DOWN(phys);
	uint64_t end = PAGE_ALIGN_UP(virt + size);

	while (v < end)
	{
		if (vmm_map_page(v, p, flags) != 0)
			return -1;

		v += PAGE_SIZE;
		p += PAGE_SIZE;
	}

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

void vmm_unmap_range(uint64_t virt, size_t size)
{
	uint64_t v = PAGE_ALIGN_DOWN(virt);
	uint64_t end = PAGE_ALIGN_UP(virt + size);

	while (v < end)
	{
		vmm_unmap_page(v);
		v += PAGE_SIZE;
	}
}

uint64_t vmm_get_physical(uint64_t virt)
{
	uint64_t *pt = vmm_get_pt(virt);
	size_t pt_idx = PT_INDEX(virt);

	if (!pte_present(pt[pt_idx]))
		return 0;

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

// ============================================================================
// Kernel Memory Allocation - Optimized with Lock Support
// ============================================================================

void *vmm_alloc_kernel_pages(size_t count)
{
	if (count == 0)
		return NULL;

	// TODO: Add spinlock here for SMP safety
	// spinlock_acquire(&g_vmm_lock);

	// Виділяємо фізичні сторінки
	uint64_t phys = pmm_alloc_pages(count);
	if (phys == 0)
	{
		// spinlock_release(&g_vmm_lock);
		return NULL;
	}

	// Резервуємо віртуальний простір
	uint64_t virt = g_kernel_heap_next;
	g_kernel_heap_next += count * PAGE_SIZE;

	// spinlock_release(&g_vmm_lock);

	// Мапимо сторінки
	if (vmm_map_range(virt, phys, count * PAGE_SIZE, PTE_WRITE | PTE_NX) != 0)
	{
		pmm_free_pages(phys, count);
		g_kernel_heap_next -= count * PAGE_SIZE;
		return NULL;
	}

	g_vmm.kernel_pages += count;

	return (void *)virt;
}

void vmm_free_kernel_pages(void *ptr, size_t count)
{
	if (!ptr || count == 0)
		return;

	uint64_t virt = (uint64_t)ptr;

	// Звільняємо кожну сторінку
	for (size_t i = 0; i < count; i++)
	{
		uint64_t phys = vmm_get_physical(virt);
		if (phys != 0)
		{
			vmm_unmap_page(virt);
			pmm_free_page(phys);
		}
		virt += PAGE_SIZE;
	}

	g_vmm.kernel_pages -= count;
}

// ============================================================================
// Bulk Operations - Optimized for Performance
// ============================================================================

/**
 * @brief Швидке копіювання page mappings (для fork)
 */
int vmm_copy_range(uint64_t dst_virt, uint64_t src_virt, size_t size, uint64_t flags)
{
	uint64_t dst = PAGE_ALIGN_DOWN(dst_virt);
	uint64_t src = PAGE_ALIGN_DOWN(src_virt);
	uint64_t end = PAGE_ALIGN_UP(src_virt + size);

	while (src < end)
	{
		uint64_t phys = vmm_get_physical(src);
		if (phys == 0)
		{
			src += PAGE_SIZE;
			dst += PAGE_SIZE;
			continue;
		}

		if (vmm_map_page(dst, phys, flags) != 0)
			return -1;

		src += PAGE_SIZE;
		dst += PAGE_SIZE;
	}

	return 0;
}

/**
 * @brief Перевірка чи вся область змапована
 */
bool vmm_is_range_mapped(uint64_t virt, size_t size)
{
	uint64_t v = PAGE_ALIGN_DOWN(virt);
	uint64_t end = PAGE_ALIGN_UP(virt + size);

	while (v < end)
	{
		if (vmm_get_physical(v) == 0)
			return false;
		v += PAGE_SIZE;
	}

	return true;
}

// ============================================================================
// Statistics and Debugging
// ============================================================================

vmm_info_t *vmm_get_info(void)
{
	// Оновлюємо статистику
	g_vmm.used_virtual_memory = g_vmm.total_mapped_pages * PAGE_SIZE;
	g_vmm.total_virtual_memory = 256ULL * 1024 * 1024 * 1024 * 1024; // 256TB

	return &g_vmm;
}

void vmm_dump_mapping(uint64_t virt)
{
	// Require printk
	// printk(KERN_DEBUG "=== Page mapping for 0x%lx ===\n", virt);

	size_t pml4_idx = PML4_INDEX(virt);
	size_t pdpt_idx = PDPT_INDEX(virt);
	size_t pd_idx = PD_INDEX(virt);
	size_t pt_idx = PT_INDEX(virt);

	uint64_t *pml4 = vmm_get_pml4();

	// printk(KERN_DEBUG "PML4[%lu] = 0x%lx\n", pml4_idx, pml4[pml4_idx]);

	if (!pte_present(pml4[pml4_idx]))
	{
		// printk(KERN_DEBUG "  Not present\n");
		return;
	}

	uint64_t *pdpt = vmm_get_pdpt(virt);
	// printk(KERN_DEBUG "PDPT[%lu] = 0x%lx\n", pdpt_idx, pdpt[pdpt_idx]);

	if (!pte_present(pdpt[pdpt_idx]))
	{
		// printk(KERN_DEBUG "  Not present\n");
		return;
	}

	uint64_t *pd = vmm_get_pd(virt);
	// printk(KERN_DEBUG "PD[%lu] = 0x%lx\n", pd_idx, pd[pd_idx]);

	if (!pte_present(pd[pd_idx]))
	{
		// printk(KERN_DEBUG "  Not present\n");
		return;
	}

	uint64_t *pt = vmm_get_pt(virt);
	// printk(KERN_DEBUG "PT[%lu] = 0x%lx\n", pt_idx, pt[pt_idx]);

	if (!pte_present(pt[pt_idx]))
	{
		// printk(KERN_DEBUG "  Not present\n");
		return;
	}

	uint64_t phys = pte_addr(pt[pt_idx]);
	// printk(KERN_DEBUG "Physical: 0x%lx\n", phys);
}

// ============================================================================
// Advanced Features
// ============================================================================

/**
 * @brief Клонування address space (для fork/exec)
 */
uint64_t vmm_clone_address_space(uint32_t flags)
{
	// Виділяємо нову PML4
	uint64_t *new_pml4 = vmm_alloc_table();
	if (!new_pml4)
		return 0;

	uint64_t *old_pml4 = vmm_get_pml4();

	// Копіюємо kernel mappings (верхня половина)
	for (size_t i = 256; i < 512; i++)
	{
		new_pml4[i] = old_pml4[i];
	}

	// User space mappings можна копіювати пізніше
	// або реалізувати copy-on-write

	return VIRT_TO_PHYS(new_pml4);
}

/**
 * @brief Перемикання address space
 */
void vmm_switch_address_space(uint64_t pml4_phys)
{
	set_cr3(pml4_phys);
	g_vmm.pml4_phys = pml4_phys;
	g_vmm.pml4_virt = (uint64_t *)PHYS_TO_VIRT(pml4_phys);
}

/**
 * @brief Знищення address space
 */
void vmm_destroy_address_space(uint64_t pml4_phys)
{
	// TODO: Рекурсивно звільнити всі user space page tables
	// Не чіпати kernel mappings!

	uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(pml4_phys);

	// Звільняємо тільки user space (0-255)
	for (size_t i = 0; i < 256; i++)
	{
		if (!pte_present(pml4[i]))
			continue;

		uint64_t pdpt_phys = pte_addr(pml4[i]);
		// Тут треба рекурсивно пройтись по всіх tables
		// та звільнити їх через pmm_free_page()
		pmm_free_page(pdpt_phys);
	}

	// Звільняємо саму PML4
	pmm_free_page(pml4_phys);
}
