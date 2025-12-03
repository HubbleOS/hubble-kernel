/**
 * @file vmm.c
 * @brief Virtual Memory Manager implementation
 */

#include "vmm.h"
#include "pmm.h"
#include "asm.h"
#include <string.h>
// #include "higher_half.h"

// ============================================================================
// Virtual Memory Layout
// ============================================================================

/**
 * Kernel Virtual Base Address
 *
 * Ядро загружается по физическому адресу 0x100000 (1MB),
 * но работает в виртуальном адресном пространстве начиная с:
 */
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

/**
 * Kernel Physical Base Address
 *
 * Физический адрес, куда загружается ядро
 */
#define KERNEL_PHYS_BASE 0x100000ULL

/**
 * Recursive Page Table Mapping
 *
 * 511-й entry PML4 указывает на саму PML4 для доступа к page tables
 */
#define RECURSIVE_MAPPING 0xFFFFFF8000000000ULL
/**
 * Heap Virtual Base
 *
 * Heap может начинаться после ядра в виртуальном пространстве
 */
#define HEAP_VIRT_START (KERNEL_VIRT_BASE + 0x10000000ULL) // +256MB

// ============================================================================
// Address Conversion Macros
// ============================================================================

/**
 * @brief Convert physical address to higher-half virtual address
 *
 * @param addr Physical address
 * @return Virtual address in kernel space
 *
 * Example:
 *   uint64_t phys = 0x200000;
 *   void *virt = PHYS_TO_VIRT(phys); // 0xFFFFFFFF80200000
 */
#define PHYS_TO_VIRT(addr) \
	((typeof(addr))((uint64_t)(addr) + KERNEL_VIRT_BASE))

/**
 * @brief Convert higher-half virtual address to physical address
 *
 * @param addr Virtual address in kernel space
 * @return Physical address
 *
 * Example:
 *   void *virt = (void*)0xFFFFFFFF80200000;
 *   uint64_t phys = VIRT_TO_PHYS(virt); // 0x200000
 */
#define VIRT_TO_PHYS(addr) \
	((typeof(addr))((uint64_t)(addr) - KERNEL_VIRT_BASE))

/**
 * @brief Check if address is in kernel virtual space
 *
 * @param addr Address to check
 * @return 1 if address is in kernel space, 0 otherwise
 */
#define IS_KERNEL_VIRT(addr) \
	((uint64_t)(addr) >= KERNEL_VIRT_BASE)

/**
 * @brief Check if address is a physical address
 *
 * @param addr Address to check
 * @return 1 if address appears to be physical, 0 otherwise
 */
#define IS_PHYSICAL(addr) \
	((uint64_t)(addr) < KERNEL_VIRT_BASE)

vmm_info_t g_vmm = {0};

// --- Internal helpers ---
static inline uint64_t pte_addr(uint64_t entry) { return entry & 0x000FFFFFFFFFF000ULL; }
static inline bool pte_present(uint64_t entry) { return entry & PTE_PRESENT; }
static inline uint64_t pte_make(uint64_t phys, uint64_t flags)
{
	uint64_t e = phys & 0x000FFFFFFFFFF000ULL;
	e |= flags & 0xFFF;
	if (flags & PTE_NX)
		e |= (1ULL << 63);
	return e;
}

static uint64_t *vmm_alloc_table(void)
{
	uint64_t phys = pmm_alloc_page();
	if (!phys)
		return NULL;
	uint64_t *virt = (uint64_t *)(phys + HIGHER_HALF_BASE);
	memset(virt, 0, VMM_PAGE_SIZE);
	return virt;
}

// --- Public API ---
void vmm_init(void)
{
	g_vmm.pml4_phys = get_cr3();
	g_vmm.pml4_virt = (uint64_t *)(g_vmm.pml4_phys + HIGHER_HALF_BASE);
	g_vmm.total_mapped_pages = 0;
}

// --- Map a virtual page to a physical page ---
int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags)
{
	uint64_t *pml4 = pml4_table();
	uint64_t *pdpt = pdpt_table(va);
	uint64_t *pd = pd_table(va);
	uint64_t *pt = pt_table(va);

	uint64_t table_flags = PTE_PRESENT | PTE_WRITE;
	if (flags & PTE_USER)
		table_flags |= PTE_USER;

	// Allocate intermediate tables if missing
	if (!pte_present(pml4[PML4_INDEX(va)]))
		pml4[PML4_INDEX(va)] = pte_make(VIRT_TO_PHYS(vmm_alloc_table()), table_flags);

	if (!pte_present(pdpt[PDPT_INDEX(va)]))
		pdpt[PDPT_INDEX(va)] = pte_make(VIRT_TO_PHYS(vmm_alloc_table()), table_flags);

	if (!pte_present(pd[PD_INDEX(va)]))
		pd[PD_INDEX(va)] = pte_make(VIRT_TO_PHYS(vmm_alloc_table()), table_flags);

	// Map huge page if aligned
	if ((pa % VMM_HUGE_PAGE_SIZE == 0) && (va % VMM_HUGE_PAGE_SIZE == 0))
	{
		pd[PD_INDEX(va)] = pte_make(pa, flags | PTE_HUGE);
		g_vmm.total_mapped_pages += VMM_HUGE_PAGE_SIZE / VMM_PAGE_SIZE;
		invlpg((void *)va);
		return 0;
	}

	// Map normal 4KB page
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
