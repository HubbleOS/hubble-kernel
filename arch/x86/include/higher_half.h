/**
 * @file higher_half.h
 * @brief Higher-Half Kernel Address Space Definitions
 *
 * Определяет виртуальное адресное пространство ядра и макросы
 * для конвертации между физическими и виртуальными адресами.
 */

#pragma once

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
