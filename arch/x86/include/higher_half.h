/**
 * @file higher_half.h
 * @brief Higher-Half Kernel Address Space Definitions
 *
 * Определяет виртуальное адресное пространство ядра и макросы
 * для конвертации между физическими и виртуальными адресами.
 */

#pragma once

#include <lib/misc.k.h>

#include <stdint.h>

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

// ============================================================================
// Page Table Manipulation
// ============================================================================

/**
 * @brief Get PML4 index from virtual address
 */
#define PML4_INDEX(virt) \
	(((uint64_t)(virt) >> 39) & 0x1FF)

/**
 * @brief Get PDPT index from virtual address
 */
#define PDPT_INDEX(virt) \
	(((uint64_t)(virt) >> 30) & 0x1FF)

/**
 * @brief Get PD index from virtual address
 */
#define PD_INDEX(virt) \
	(((uint64_t)(virt) >> 21) & 0x1FF)

/**
 * @brief Get PT index from virtual address
 */
#define PT_INDEX(virt) \
	(((uint64_t)(virt) >> 12) & 0x1FF)

/**
 * @brief Get page offset from virtual address
 */
#define PAGE_OFFSET(virt) \
	((uint64_t)(virt) & 0xFFF)

/**
 * @brief Align address down to page boundary
 */
#define PAGE_ALIGN_DOWN(addr) ALIGN_DOWN(addr, PAGE_SIZE)

/**
 * @brief Align address up to page boundary
 */
#define PAGE_ALIGN_UP(addr) ALIGN_UP(addr, PAGE_SIZE)

/**
 * @brief Align address down to 2MB boundary (huge page)
 */
#define HUGE_PAGE_ALIGN_DOWN(addr) ALIGN_DOWN(addr, 2 * 1024 * 1024)

/**
 * @brief Align address up to 2MB boundary (huge page)
 */
#define HUGE_PAGE_ALIGN_UP(addr) ALIGN_UP(addr, 2 * 1024 * 1024)

// ============================================================================
// Recursive Page Table Access
// ============================================================================

/**
 * @brief Access PML4 through recursive mapping
 *
 * Entry 511 of PML4 points to PML4 itself, allowing access
 * to all page table levels through virtual addresses
 */
#define RECURSIVE_PML4 \
	((uint64_t *)(RECURSIVE_MAPPING | (511ULL << 30) | (511ULL << 21) | (511ULL << 12)))

/**
 * @brief Access PDPT through recursive mapping
 *
 * @param pml4_idx PML4 index
 */
#define RECURSIVE_PDPT(pml4_idx) \
	((uint64_t *)(RECURSIVE_MAPPING | (511ULL << 30) | (511ULL << 21) | ((pml4_idx) << 12)))

/**
 * @brief Access PD through recursive mapping
 *
 * @param pml4_idx PML4 index
 * @param pdpt_idx PDPT index
 */
#define RECURSIVE_PD(pml4_idx, pdpt_idx) \
	((uint64_t *)(RECURSIVE_MAPPING | (511ULL << 30) | ((pml4_idx) << 21) | ((pdpt_idx) << 12)))

/**
 * @brief Access PT through recursive mapping
 *
 * @param pml4_idx PML4 index
 * @param pdpt_idx PDPT index
 * @param pd_idx PD index
 */
#define RECURSIVE_PT(pml4_idx, pdpt_idx, pd_idx) \
	((uint64_t *)(RECURSIVE_MAPPING | ((pml4_idx) << 30) | ((pdpt_idx) << 21) | ((pd_idx) << 12)))

// ============================================================================
// Page Table Entry Flags
// ============================================================================

#define PTE_PRESENT (1ULL << 0)	     // Page is present in memory
#define PTE_WRITE (1ULL << 1)	     // Page is writable
#define PTE_USER (1ULL << 2)	     // User mode access allowed
#define PTE_WRITETHROUGH (1ULL << 3) // Write-through caching
#define PTE_NOCACHE (1ULL << 4)	     // Disable caching
#define PTE_ACCESSED (1ULL << 5)     // Page has been accessed
#define PTE_DIRTY (1ULL << 6)	     // Page has been written to
#define PTE_HUGE (1ULL << 7)	     // Huge page (2MB/1GB)
#define PTE_GLOBAL (1ULL << 8)	     // Global page (not flushed on CR3 reload)
#define PTE_NX (1ULL << 63)	     // No execute

/**
 * @brief Extract physical address from page table entry
 */
#define PTE_ADDR(entry) \
	((entry) & 0x000FFFFFFFFFF000ULL)

/**
 * @brief Extract flags from page table entry
 */
#define PTE_FLAGS(entry) \
	((entry) & 0xFFF0000000000FFFULL)

// ============================================================================
// Helper Functions (defined in kernel_entry.c or mm/vmm.c)
// ============================================================================

#ifndef __ASSEMBLER__

/**
 * @brief Convert physical address to virtual address
 *
 * @param phys_addr Physical address
 * @return Virtual address in kernel space
 */
void *phys_to_virt(uint64_t phys_addr);

/**
 * @brief Convert virtual address to physical address
 *
 * @param virt_addr Virtual address
 * @return Physical address (0 if address is not in kernel space)
 */
uint64_t virt_to_phys(void *virt_addr);

/**
 * @brief Validate that address is properly in kernel space
 *
 * @param addr Address to validate
 * @return 1 if valid kernel address, 0 otherwise
 */
static inline int is_kernel_address(void *addr)
{
	return IS_KERNEL_VIRT(addr);
}

/**
 * @brief Get current CR3 value (physical address of PML4)
 *
 * @return Physical address of current page table
 */
static inline uint64_t get_cr3(void)
{
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	return cr3;
}

/**
 * @brief Set CR3 value (switch page tables)
 *
 * @param pml4_phys Physical address of new PML4
 */
static inline void set_cr3(uint64_t pml4_phys)
{
	asm volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

/**
 * @brief Flush TLB entry for specific virtual address
 *
 * @param virt Virtual address to flush
 */
static inline void invlpg(void *virt)
{
	asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

#endif /* __ASSEMBLER__ */
