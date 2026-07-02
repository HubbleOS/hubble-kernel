/**
 * @file vmm.h
 * @brief Virtual Memory Manager - Page table management and virtual address
 *        space handling
 *
 * Implements recursive page tables for x86-64 higher-half kernel with
 * user-space support.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -- Page Table Constants -------------------------------------------------- */

#define VMM_PAGE_SIZE 4096
#define VMM_HUGE_PAGE_SIZE (2 * 1024 * 1024)

/* -- Page Table Access via Recursive Mapping ------------------------------- */

#define PML4_INDEX(va) (((uint64_t)(va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((uint64_t)(va) >> 30) & 0x1FF)
#define PD_INDEX(va) (((uint64_t)(va) >> 21) & 0x1FF)
#define PT_INDEX(va) (((uint64_t)(va) >> 12) & 0x1FF)

#define RECURSIVE_INDEX 510ULL
#define HIGHER_HALF_BASE 0xFFFFFFFF80000000ULL

#define RECURSIVE_PML4_INDEX 510
#define RECURSIVE_MAPPING                                                      \
  (0xFFFFULL << 48 | (uint64_t)RECURSIVE_PML4_INDEX << 39)

/* -- Recursive Page Table Helpers ------------------------------------------ */

/**
 * @brief Get the virtual address of the PML4 table
 *
 * @return Pointer to the PML4 table
 */
static inline uint64_t *pml4_table(void) {
  return (uint64_t *)(RECURSIVE_MAPPING |
                      ((uint64_t)RECURSIVE_PML4_INDEX << 30) |
                      ((uint64_t)RECURSIVE_PML4_INDEX << 21) |
                      ((uint64_t)RECURSIVE_PML4_INDEX << 12));
}

/**
 * @brief Get the virtual address of the PDPT for a given virtual address
 *
 * @param va Virtual address
 * @return Pointer to the PDPT
 */
static inline uint64_t *pdpt_table(uint64_t va) {
  return (uint64_t *)(RECURSIVE_MAPPING |
                      ((uint64_t)RECURSIVE_PML4_INDEX << 30) |
                      ((uint64_t)RECURSIVE_PML4_INDEX << 21) |
                      (PML4_INDEX(va) << 12));
}

/**
 * @brief Get the virtual address of the PD for a given virtual address
 *
 * @param va Virtual address
 * @return Pointer to the PD
 */
static inline uint64_t *pd_table(uint64_t va) {
  return (uint64_t *)(RECURSIVE_MAPPING |
                      ((uint64_t)RECURSIVE_PML4_INDEX << 30) |
                      (PML4_INDEX(va) << 21) | (PDPT_INDEX(va) << 12));
}

/**
 * @brief Get the virtual address of the PT for a given virtual address
 *
 * @param va Virtual address
 * @return Pointer to the PT
 */
static inline uint64_t *pt_table(uint64_t va) {
  return (uint64_t *)(RECURSIVE_MAPPING | (PML4_INDEX(va) << 30) |
                      (PDPT_INDEX(va) << 21) | (PD_INDEX(va) << 12));
}

/* -- Page Table Entry Flags ------------------------------------------------ */

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_HUGE (1ULL << 7)
#define PTE_GLOBAL (1ULL << 8)
#define PTE_NX (1ULL << 63)

#define PTE_NOCACHE (PTE_PWT | PTE_PCD)

#define VMM_FLAGS_KERNEL (PTE_PRESENT | PTE_WRITE)
#define VMM_FLAGS_USER (PTE_PRESENT | PTE_WRITE | PTE_USER)
#define VMM_FLAGS_USER_RO (PTE_PRESENT | PTE_USER)
#define VMM_FLAGS_STACK (PTE_PRESENT | PTE_WRITE | PTE_NX)
#define VMM_FLAGS_HEAP (PTE_PRESENT | PTE_WRITE | PTE_NX)
#define VMM_FLAGS_NO_CACHE (PTE_PRESENT | PTE_WRITE | PTE_NOCACHE)
#define VMM_FLAGS_GLOBAL (PTE_PRESENT | PTE_WRITE | PTE_GLOBAL)

#define VMM_MAP_NO_CACHE (1ULL << 0)
#define VMM_MAP_GLOBAL (1ULL << 1)
#define VMM_MAP_USER (1ULL << 2)

/* -- VMM State ------------------------------------------------------------- */

/**
 * @brief Virtual Memory Manager state
 */
typedef struct {
  uint64_t *pml4_virt;
  uint64_t pml4_phys;
  uint64_t total_mapped_pages;
} vmm_info_t;

extern vmm_info_t g_vmm;

/* -- Core API -------------------------------------------------------------- */

/**
 * @brief Initialize the Virtual Memory Manager
 *
 * Reads CR3 and stores the current page table information.
 */
void vmm_init(void);

/**
 * @brief Map a virtual page to a physical page
 *
 * @param va Virtual address
 * @param pa Physical address
 * @param flags Page table entry flags (PTE_*)
 * @return 0 on success, -1 on failure
 */
int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags);

/**
 * @brief Unmap a virtual page
 *
 * @param va Virtual address to unmap
 */
void vmm_unmap_page(uint64_t va);

/**
 * @brief Change page table entry flags for an existing mapping
 *
 * @param va Virtual address
 * @param flags New PTE flags
 * @return 0 on success, -1 if not mapped
 */
int vmm_set_flags(uint64_t va, uint64_t flags);

/**
 * @brief Get the physical address mapped to a virtual address
 *
 * @param va Virtual address
 * @return Physical address, or 0 if not mapped
 */
uint64_t vmm_get_phys(uint64_t va);

/**
 * @brief Check if a virtual address is mapped
 *
 * @param va Virtual address
 * @return true if mapped, false otherwise
 */
bool vmm_is_mapped(uint64_t va);

/**
 * @brief Unmap a user page from the current page table
 *
 * @param va Virtual address to unmap
 */
void vmm_unmap_user_page(uint64_t va);

/**
 * @brief Dump page flags for a virtual address
 *
 * @param va Virtual address
 */
void dump_page_flags(uint64_t va);

/**
 * @brief Dump memory content at a virtual address
 *
 * @param va Virtual address
 * @param len Number of bytes to dump
 */
void dump_page(uint64_t va, size_t len);

/**
 * @brief Set the USER flag on a PD entry (2 MB page)
 *
 * @param va Virtual address within the 2 MB page
 * @return 0 on success, -1 on failure
 */
int make_pd_entry_user(uint64_t va);

/**
 * @brief Create a user-space page table cloned from the kernel
 *
 * @return Physical address of the new PML4, or NULL on failure
 */
uint64_t *vmm_create_user_pagemap(void);

/**
 * @brief Map a page into a specific page table
 *
 * @param pml4_phys Physical address of the target PML4
 * @param va Virtual address
 * @param pa Physical address
 * @param flags Page table entry flags
 * @return 0 on success, -1 on failure
 */
int vmm_map_page_into(uint64_t *pml4_phys, uint64_t va, uint64_t pa,
                      uint64_t flags);

/**
 * @brief Get physical address from a specific page table
 *
 * @param pml4_phys Physical address of the PML4
 * @param va Virtual address
 * @return Physical address, or 0 if not mapped
 */
uint64_t vmm_get_phys_from(uint64_t *pml4_phys, uint64_t va);

/**
 * @brief Debug: dump full page table walk for a virtual address
 *
 * @param pml4_phys Physical address of the PML4
 * @param va Virtual address
 */
void debug_dump_mapping(uint64_t *pml4_phys, uint64_t va);

/**
 * @brief Dump the entire kernel page table hierarchy
 */
void dump_kernel_pagemap(void);
