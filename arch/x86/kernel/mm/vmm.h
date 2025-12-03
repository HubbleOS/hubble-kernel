/**
 * @file vmm.h
 * @brief Virtual Memory Manager (VMM) header
 *
 * This file defines the core structures, constants, and functions for the
 * Virtual Memory Manager (VMM) in the x86 architecture.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- Конфигурация ---
#define VMM_PAGE_SIZE 4096
#define VMM_HUGE_PAGE_SIZE (2 * 1024 * 1024)

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

// --- Common combinations ---
#define VMM_FLAGS_KERNEL (PTE_PRESENT | PTE_WRITE)
#define VMM_FLAGS_USER (PTE_PRESENT | PTE_WRITE | PTE_USER)
#define VMM_FLAGS_USER_RO (PTE_PRESENT | PTE_USER)
#define VMM_FLAGS_STACK (PTE_PRESENT | PTE_WRITE | PTE_NX)
#define VMM_FLAGS_HEAP (PTE_PRESENT | PTE_WRITE | PTE_NX)

// --- VMM state ---
typedef struct
{
	uint64_t *pml4_virt;
	uint64_t pml4_phys;
	uint64_t total_mapped_pages;
} vmm_info_t;

extern vmm_info_t g_vmm;

// --- Core API ---
void vmm_init(void);
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t virt);
int vmm_set_flags(uint64_t virt, uint64_t flags);
