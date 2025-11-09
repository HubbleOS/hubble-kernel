// #pragma once

// #include <stdint.h>
// #include <stddef.h>

// #define PAGE_SIZE 0x1000

// #define PHYSMAP_BASE 0xFFFF800000000000ULL

// // Flags for PTE
// #define PTE_PRESENT (1ULL << 0)
// #define PTE_WRITABLE (1ULL << 1)
// #define PTE_USER (1ULL << 2)
// #define PTE_PWT (1ULL << 3)
// #define PTE_PCD (1ULL << 4)
// #define PTE_ACCESSED (1ULL << 5)
// #define PTE_DIRTY (1ULL << 6)
// #define PTE_PS (1ULL << 7)
// #define PTE_GLOBAL (1ULL << 8)
// #define PTE_NX (1ULL << 63)

// // Convenience macros
// #define VMM_PRESENT PTE_PRESENT
// #define VMM_WRITE PTE_WRITABLE
// #define VMM_USER PTE_USER
// #define VMM_NX PTE_NX

// void vmm_init(uint64_t bootstrap_cr3_phys, uint64_t heap_start, uint64_t heap_size);
// uint64_t vmm_get_current_cr3_phys(void);
// void vmm_switch_cr3(uint64_t phys);

// int vmm_map(uint64_t virt, uint64_t phys, size_t pages, uint64_t flags);
// // int vmm_unmap(uint64_t virt, size_t pages);
// int vmm_unmap(uint64_t virt, size_t pages, int free_pages);
// uint64_t vmm_translate(uint64_t virt);

// void vmm_test(void);

// // Bootstrap allocator for init
// void vmm_set_bootstrap_allocator(uint64_t base, uint64_t size);
// void vmm_disable_bootstrap_allocator(void);

// // For user space
// uint64_t vmm_alloc_physical_page(void);
// void vmm_free_physical_page(uint64_t phys);
// int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

#pragma once

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 0x1000
#define PHYSMAP_BASE 0xFFFF800000000000ULL

// Page table entry flags
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_WRITETHROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_PS (1ULL << 7) // Page Size (for large pages)
#define PTE_GLOBAL (1ULL << 8)
#define PTE_NX (1ULL << 63)

// Initialize VMM with separate bootstrap allocator and heap
void vmm_init(uint64_t bootstrap_cr3_phys,
	      uint64_t bootstrap_start, uint64_t bootstrap_size,
	      uint64_t heap_start, uint64_t heap_size);

// Bootstrap allocator management
void vmm_set_bootstrap_allocator(uint64_t base, uint64_t size);
void vmm_disable_bootstrap_allocator(void);

// CR3 management
uint64_t vmm_get_current_cr3_phys(void);
void vmm_switch_cr3(uint64_t phys);

// Mapping functions
int vmm_map(uint64_t virt, uint64_t phys, size_t pages, uint64_t flags);
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap(uint64_t virt, size_t pages, int free_pages);

// Translation
uint64_t vmm_translate(uint64_t virt);

// Page table cleanup
void vmm_cleanup_empty_tables(uint64_t virt);

// Physical page allocation (used by user space management)
uint64_t vmm_alloc_physical_page(void);
void vmm_free_physical_page(uint64_t phys);
