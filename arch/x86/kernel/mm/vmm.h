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
// Page Table Entry Flags (дублюємо з higher_half.h для зручності)
// ============================================================================

#define PTE_PRESENT (1ULL << 0)	     // Page is present
#define PTE_WRITE (1ULL << 1)	     // Page is writable
#define PTE_USER (1ULL << 2)	     // User mode access
#define PTE_WRITETHROUGH (1ULL << 3) // Write-through caching
#define PTE_NOCACHE (1ULL << 4)	     // Disable caching
#define PTE_ACCESSED (1ULL << 5)     // Page accessed
#define PTE_DIRTY (1ULL << 6)	     // Page written to
#define PTE_HUGE (1ULL << 7)	     // Huge page (2MB/1GB)
#define PTE_GLOBAL (1ULL << 8)	     // Global page
#define PTE_NX (1ULL << 63)	     // No execute

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

	bool initialized; // True if VMM is initialized
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
 * @brief Check if VMM is initialized
 *
 * @return True if VMM is initialized, false otherwise
 */
bool vmm_is_initialized(void);

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
 * @brief Map range of pages
 *
 * @param virt_start Virtual start address
 * @param phys_start Physical start address
 * @param size Size in bytes (will be page-aligned)
 * @param flags Page flags
 * @return 0 on success, -1 on error
 *
 * Example:
 *   // Map 4MB region
 *   vmm_map_range(0xFFFFFFFF80600000, 0x600000, 4*1024*1024, PTE_WRITE);
 */
int vmm_map_range(uint64_t virt_start, uint64_t phys_start, size_t size, uint64_t flags);

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

// ============================================================================
// Kernel Memory Allocation
// ============================================================================

/**
 * @brief Allocate pages in kernel space
 *
 * Allocates physical pages and maps them in kernel virtual space.
 * Returns virtual address ready to use.
 *
 * @param count Number of pages to allocate
 * @return Virtual address of allocated region or NULL on error
 *
 * Example:
 *   void *buffer = vmm_alloc_kernel_pages(16); // 64KB buffer
 *   if (buffer) {
 *       // Use buffer...
 *       vmm_free_kernel_pages(buffer, 16);
 *   }
 */
void *vmm_alloc_kernel_pages(size_t count);

/**
 * @brief Free kernel pages
 *
 * Frees both virtual mapping and physical memory.
 *
 * @param virt_addr Virtual address (from vmm_alloc_kernel_pages)
 * @param count Number of pages to free
 */
void vmm_free_kernel_pages(void *virt_addr, size_t count);

// ============================================================================
// Information and Debugging
// ============================================================================

/**
 * @brief Get VMM statistics
 *
 * @return Pointer to VMM info structure
 */
vmm_info_t *vmm_get_info(void);

/**
 * @brief Dump mapping info for virtual address
 *
 * Prints page table hierarchy for debugging.
 *
 * @param virt_addr Virtual address to inspect
 */
void vmm_dump_mapping(uint64_t virt_addr);

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Check if page is mapped
 */
#define vmm_is_mapped(virt) \
	(vmm_get_physical(virt) != 0)

/**
 * @brief Map physical memory to kernel space
 */
#define vmm_map_physical(phys, size, flags) \
	vmm_map_range(PHYS_TO_VIRT(phys), phys, size, flags)

/**
 * @brief Map device memory (uncached)
 */
#define vmm_map_device(virt, phys, size) \
	vmm_map_range(virt, phys, size, VMM_FLAGS_DEVICE)

/**
 * @brief Map kernel code (read-only)
 */
#define vmm_map_code(virt, phys, size) \
	vmm_map_range(virt, phys, size, VMM_FLAGS_CODE)

/**
 * @brief Map kernel data (read-write)
 */
#define vmm_map_data(virt, phys, size) \
	vmm_map_range(virt, phys, size, VMM_FLAGS_KERNEL)

// ============================================================================
// Bulk Operations
// ============================================================================

/**
 * @brief Copy page mappings from one range to another
 *
 * @param dst_virt Destination virtual address
 * @param src_virt Source virtual address
 * @param size Size in bytes
 * @param flags Flags for destination mappings
 * @return 0 on success, -1 on error
 */
int vmm_copy_range(uint64_t dst_virt, uint64_t src_virt, size_t size, uint64_t flags);

/**
 * @brief Check if entire range is mapped
 *
 * @param virt Virtual address start
 * @param size Size in bytes
 * @return true if all pages mapped, false otherwise
 */
bool vmm_is_range_mapped(uint64_t virt, size_t size);

// ============================================================================
// Advanced Functions
// ============================================================================

/**
 * @brief Clone address space for new process
 *
 * Creates new page tables with kernel mappings copied.
 * User space can be copied or marked copy-on-write.
 *
 * @param flags Clone flags (COW, shared, etc.)
 * @return Physical address of new PML4 or 0 on error
 */
uint64_t vmm_clone_address_space(uint32_t flags);

/**
 * @brief Switch to different address space
 *
 * @param pml4_phys Physical address of PML4 to switch to
 */
void vmm_switch_address_space(uint64_t pml4_phys);

/**
 * @brief Destroy address space
 *
 * Frees all user space page tables and the PML4.
 * Does NOT touch kernel mappings.
 *
 * @param pml4_phys Physical address of PML4 to destroy
 */
void vmm_destroy_address_space(uint64_t pml4_phys);

// ============================================================================
// TLB Management
// ============================================================================

/**
 * @brief Flush entire TLB
 */
static inline void vmm_flush_tlb(void)
{
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/**
 * @brief Flush TLB entry for specific address
 */
static inline void vmm_flush_tlb_single(uint64_t virt_addr)
{
	asm volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

_End_C_Header;
