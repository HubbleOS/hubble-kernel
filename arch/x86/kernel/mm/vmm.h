/**
 * @file vmm.h
 * @brief Virtual Memory Manager (VMM) header
 *
 * Updated for recursive page tables and higher-half kernel with user-space support.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Page Table Constants
// ============================================================================
#define VMM_PAGE_SIZE 4096
#define VMM_HUGE_PAGE_SIZE (2 * 1024 * 1024)

// --- Index macros ---
#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va) (((va) >> 21) & 0x1FF)
#define PT_INDEX(va) (((va) >> 12) & 0x1FF)

// --- Recursive mapping ---
#define RECURSIVE_INDEX 510ULL
#define HIGHER_HALF_BASE 0xFFFFFFFF80000000ULL

// ============================================================================
// Page Table Entry Flags
// ============================================================================
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_WRITETHROUGH (1ULL << 3)
#define PTE_NOCACHE (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_HUGE (1ULL << 7)
#define PTE_GLOBAL (1ULL << 8)
#define PTE_NX (1ULL << 63)

// --- Common flag sets ---
#define VMM_FLAGS_KERNEL (PTE_PRESENT | PTE_WRITE)
#define VMM_FLAGS_USER (PTE_PRESENT | PTE_WRITE | PTE_USER)
#define VMM_FLAGS_USER_RO (PTE_PRESENT | PTE_USER)
#define VMM_FLAGS_STACK (PTE_PRESENT | PTE_WRITE | PTE_NX)
#define VMM_FLAGS_HEAP (PTE_PRESENT | PTE_WRITE | PTE_NX)

// ============================================================================
// VMM State
// ============================================================================
typedef struct
{
	uint64_t *pml4_virt;	     // Virtual address of PML4 (higher-half)
	uint64_t pml4_phys;	     // Physical address of PML4
	uint64_t total_mapped_pages; // Counter of mapped pages
} vmm_info_t;

extern vmm_info_t g_vmm;

// ============================================================================
// Core API
// ============================================================================
/**
 * @brief Initialize VMM state (reads CR3 and maps higher-half PML4)
 */
void vmm_init(void);

/**
 * @brief Map a virtual page to a physical page
 *
 * @param virt Virtual address
 * @param phys Physical address
 * @param flags Page flags (PTE_*)
 * @return 0 on success, -1 on failure
 */
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

/**
 * @brief Unmap a virtual page
 *
 * @param virt Virtual address to unmap
 */
void vmm_unmap_page(uint64_t virt);

/**
 * @brief Change page flags of an existing mapped page
 *
 * @param virt Virtual address
 * @param flags New PTE flags
 * @return 0 on success, -1 if page not present
 */
int vmm_set_flags(uint64_t virt, uint64_t flags);

// ============================================================================
// Helper macros for internal use (recursive page tables)
// ============================================================================
static inline uint64_t *pml4_table(void)
{
	return (uint64_t *)(RECURSIVE_INDEX << 39);
}
static inline uint64_t *pdpt_table(uint64_t va)
{
	return (uint64_t *)((RECURSIVE_INDEX << 39) | (PML4_INDEX(va) << 30));
}
static inline uint64_t *pd_table(uint64_t va)
{
	return (uint64_t *)((RECURSIVE_INDEX << 39) | (PML4_INDEX(va) << 30) | (PDPT_INDEX(va) << 21));
}
static inline uint64_t *pt_table(uint64_t va)
{
	return (uint64_t *)((RECURSIVE_INDEX << 39) | (PML4_INDEX(va) << 30) | (PDPT_INDEX(va) << 21) | (PD_INDEX(va) << 12));
}
