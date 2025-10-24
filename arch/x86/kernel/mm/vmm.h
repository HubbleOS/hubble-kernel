#pragma once

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 0x1000

// Flags for PTE
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_PS (1ULL << 7)
#define PTE_GLOBAL (1ULL << 8)
#define PTE_NX (1ULL << 63)

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

void vmm_init(uint64_t bootstrap_cr3_phys, uint64_t heap_start, uint64_t heap_size);
uint64_t vmm_get_current_cr3_phys(void);
void vmm_switch_cr3(uint64_t phys);

int vmm_map(uint64_t virt, uint64_t phys, size_t pages, uint64_t flags);
int vmm_unmap(uint64_t virt, size_t pages);
uint64_t vmm_translate(uint64_t virt);

void vmm_test(void);

// Bootstrap allocator for init
void vmm_set_bootstrap_allocator(uint64_t base, uint64_t size);
void vmm_disable_bootstrap_allocator(void);
