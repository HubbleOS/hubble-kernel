/**
 * @file vmm.h
 * @brief Virtual Memory Manager - Higher-Half Kernel Edition
 *
 * Керує віртуальною пам'яттю в higher-half kernel mode.
 * Використовує recursive mapping для доступу до page tables.
 */

#pragma once

#include <_cheader.h>

_Begin_C_Header;

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Virtual Memory Configuration
// ============================================================================

#define VMM_PAGE_SIZE 4096
#define VMM_HUGE_PAGE_SIZE (2 * 1024 * 1024) // 2MB

// ============================================================================
// Page Table Entry Flags
// ============================================================================

// #define PTE_PRESENT (1ULL << 0)	     // Page is present
// #define PTE_WRITE (1ULL << 1)	     // Page is writable
// #define PTE_USER (1ULL << 2)	     // User mode access
// #define PTE_WRITETHROUGH (1ULL << 3) // Write-through caching
// #define PTE_NOCACHE (1ULL << 4)	     // Disable caching
// #define PTE_ACCESSED (1ULL << 5)     // Page accessed
// #define PTE_DIRTY (1ULL << 6)	     // Page written to
// #define PTE_HUGE (1ULL << 7)	     // Huge page (2MB/1GB)
// #define PTE_GLOBAL (1ULL << 8)	     // Global page
// #define PTE_NX (1ULL << 63)	     // No execute

// ============================================================================
// Common flag combinations
// ============================================================================

#define VMM_FLAGS_KERNEL (PTE_WRITE)			     // Kernel RW
#define VMM_FLAGS_KERNEL_RO (0)				     // Kernel R
#define VMM_FLAGS_USER (PTE_WRITE | PTE_USER)		     // User RW
#define VMM_FLAGS_USER_RO (PTE_USER)			     // User R
#define VMM_FLAGS_DEVICE (PTE_WRITE | PTE_NOCACHE)	     // Device memory
#define VMM_FLAGS_CODE (0)				     // Executable code
#define VMM_FLAGS_CODE_NX (PTE_NX)			     // Non-executable data
#define VMM_FLAGS_STACK (PTE_WRITE | PTE_NX)		     // Stack
#define VMM_FLAGS_HEAP (PTE_WRITE | PTE_NX)		     // Heap
#define VMM_FLAGS_GLOBAL (PTE_WRITE | PTE_GLOBAL)	     // Global mapping
#define VMM_FLAGS_USER_STACK (PTE_WRITE | PTE_USER | PTE_NX) // User stack
#define VMM_FLAGS_USER_HEAP (PTE_WRITE | PTE_USER | PTE_NX)  // User heap

// ============================================================================
// VMM Information Structure
// ============================================================================

typedef struct
{
	uint64_t *pml4_virt;	       // Virtual address of PML4
	uint64_t pml4_phys;	       // Physical address of PML4
	uint64_t total_mapped_pages;   // Total mapped pages
	uint64_t kernel_pages;	       // Kernel allocated pages
	uint64_t user_pages;	       // User allocated pages
	uint64_t total_virtual_memory; // Total virtual address space (256TB)
	uint64_t used_virtual_memory;  // Used virtual memory
} vmm_info_t;

// ============================================================================
// VMM Core Functions
// ============================================================================

/**
 * @brief Initialize Virtual Memory Manager
 *
 * Встановлює VMM для роботи з існуючими page tables,
 * створеними bootloader'ом. Використовує recursive mapping
 * для доступу до page tables.
 */
void vmm_init(void);

/**
 * @brief Map single page
 *
 * @param virt_addr Virtual address (must be page-aligned)
 * @param phys_addr Physical address (must be page-aligned)
 * @param flags Page flags (PTE_WRITE, PTE_USER, etc.)
 * @return 0 on success, -1 on error
 *
 * Example:
 *   vmm_map_page(0xFFFFFFFF80500000, 0x500000, PTE_WRITE);
 */
int vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

/**
 * @brief Unmap single page
 *
 * @param virt_addr Virtual address to unmap
 *
 * Note: Does NOT free physical memory - use pmm_free_page() separately
 */
void vmm_unmap_page(uint64_t virt_addr);

/**
 * @brief Unmap range of pages
 *
 * @param virt_start Virtual start address
 * @param size Size in bytes
 *
 * Note: Does NOT free physical memory
 */
void vmm_unmap_range(uint64_t virt_start, size_t size);

/**
 * @brief Get physical address for virtual address
 *
 * @param virt_addr Virtual address
 * @return Physical address or 0 if not mapped
 *
 * Example:
 *   uint64_t phys = vmm_get_physical(0xFFFFFFFF80500123);
 *   // Returns 0x500123 if mapped
 */
uint64_t vmm_get_physical(uint64_t virt_addr);

/**
 * @brief Change page flags
 *
 * @param virt_addr Virtual address
 * @param flags New flags
 * @return 0 on success, -1 on error
 *
 * Example:
 *   // Make page read-only
 *   vmm_set_flags(addr, 0);
 *   // Make page writable
 *   vmm_set_flags(addr, PTE_WRITE);
 */
int vmm_set_flags(uint64_t virt_addr, uint64_t flags);

_End_C_Header;
