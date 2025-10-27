#include "vmm.h"
#include "pmm.h"
#include "printk.h"

#include <string.h>

static inline void *phys_to_virt(uint64_t phys)
{
	return (void *)phys;
}

static inline uint64_t virt_to_phys(void *vptr)
{
	return (uint64_t)vptr;
}

static uint64_t current_pml4_phys = 0;

// Bootstrap allocator
static uint64_t bootstrap_alloc_base = 0;
static uint64_t bootstrap_alloc_offset = 0;
static uint64_t bootstrap_alloc_size = 0;
static int use_bootstrap = 0;

void vmm_set_bootstrap_allocator(uint64_t base, uint64_t size)
{
	bootstrap_alloc_base = base;
	bootstrap_alloc_size = size;
	bootstrap_alloc_offset = 0;
	use_bootstrap = 1;
	printk("Bootstrap allocator: 0x%llx - 0x%llx (%llu KB)\n",
	       base, base + size, size / 1024);
}

void vmm_disable_bootstrap_allocator(void)
{
	use_bootstrap = 0;
	printk("Bootstrap allocator disabled (used %llu bytes)\n",
	       bootstrap_alloc_offset);
}

uint64_t vmm_get_current_cr3_phys(void)
{
	return current_pml4_phys;
}

void vmm_switch_cr3(uint64_t phys)
{
	current_pml4_phys = phys;
	asm volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

static uint64_t alloc_table_phys(void)
{
	if (use_bootstrap)
	{
		if (bootstrap_alloc_offset + PAGE_SIZE > bootstrap_alloc_size)
		{
			printk("ERROR: Bootstrap allocator exhausted!\n");
			return 0;
		}

		uint64_t phys = bootstrap_alloc_base + bootstrap_alloc_offset;
		bootstrap_alloc_offset += PAGE_SIZE;

		void *virt = phys_to_virt(phys);
		memset(virt, 0, PAGE_SIZE);
		return phys;
	}

	// After disabling bootstrap - use PMM
	void *virt = pmm_alloc(1);
	if (!virt)
		return 0;

	memset(virt, 0, PAGE_SIZE);
	return pmm_get_phys(virt);
}

static inline size_t idx_from_virt(uint64_t virt, int level)
{
	int shift = 12 + (level - 1) * 9;
	return (virt >> shift) & 0x1FF;
}

static uint64_t *get_pte_for(uint64_t pml4_phys, uint64_t virt, int create)
{
	uint64_t phys = pml4_phys;

	for (int level = 4; level > 1; level--)
	{
		uint64_t *table = (uint64_t *)phys_to_virt(phys);
		size_t idx = idx_from_virt(virt, level);
		uint64_t entry = table[idx];

		if (!(entry & PTE_PRESENT))
		{
			if (!create)
				return NULL;

			uint64_t new_phys = alloc_table_phys();
			if (!new_phys)
			{
				printk("ERROR: Failed to allocate page table at level %d\n", level);
				return NULL;
			}

			table[idx] = new_phys | PTE_PRESENT | PTE_WRITABLE;
			entry = table[idx];
		}

		phys = entry & 0x000FFFFFFFFFF000ULL;
	}

	uint64_t *pt = (uint64_t *)phys_to_virt(phys);
	size_t tidx = idx_from_virt(virt, 1);
	return &pt[tidx];
}

int vmm_map(uint64_t virt, uint64_t phys, size_t pages, uint64_t flags)
{
	if (virt & (PAGE_SIZE - 1))
		return -1;
	if (phys & (PAGE_SIZE - 1))
		return -1;

	for (size_t i = 0; i < pages; i++)
	{
		uint64_t *pte = get_pte_for(current_pml4_phys, virt + i * PAGE_SIZE, 1);
		if (!pte)
			return -2;

		*pte = (phys + i * PAGE_SIZE) | (flags & 0xFFF) | PTE_PRESENT;
	}

	for (size_t i = 0; i < pages; i++)
	{
		asm volatile("invlpg (%0)" : : "r"(virt + i * PAGE_SIZE) : "memory");
	}

	return 0;
}

int vmm_unmap(uint64_t virt, size_t pages)
{
	if (virt & (PAGE_SIZE - 1))
		return -1;

	for (size_t i = 0; i < pages; i++)
	{
		uint64_t *pte = get_pte_for(current_pml4_phys, virt + i * PAGE_SIZE, 0);
		if (!pte)
			continue;

		*pte = 0;
		asm volatile("invlpg (%0)" : : "r"(virt + i * PAGE_SIZE) : "memory");
	}

	return 0;
}

uint64_t vmm_translate(uint64_t virt)
{
	uint64_t phys = current_pml4_phys;

	for (int level = 4; level >= 1; level--)
	{
		uint64_t *table = (uint64_t *)phys_to_virt(phys);
		size_t idx = idx_from_virt(virt, level);
		uint64_t entry = table[idx];

		if (!(entry & PTE_PRESENT))
			return 0;

		if (level > 1 && (entry & PTE_PS))
		{
			uint64_t base = entry & 0x000FFFFFFFFFF000ULL;
			int shift = 12 + (level - 1) * 9;
			uint64_t offset = virt & ((1ULL << shift) - 1);
			return base + offset;
		}

		phys = entry & 0x000FFFFFFFFFF000ULL;
	}

	uint64_t *pt = (uint64_t *)phys_to_virt(phys);
	size_t tidx = idx_from_virt(virt, 1);
	uint64_t e = pt[tidx];

	if (!(e & PTE_PRESENT))
		return 0;

	return (e & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

void vmm_init(uint64_t bootstrap_cr3_phys, uint64_t heap_start, uint64_t heap_size)
{
	printk("=== VMM Init ===\n");
	printk("Bootstrap CR3: 0x%llx\n", bootstrap_cr3_phys);
	printk("Heap: 0x%llx - 0x%llx (%llu MB)\n",
	       heap_start, heap_start + heap_size, heap_size / (1024 * 1024));

	if (!bootstrap_cr3_phys)
	{
		printk("FATAL: No bootstrap CR3 provided\n");
		return;
	}

	// Check minimum heap size
	if (heap_size < 16 * 1024 * 1024)
	{
		printk("FATAL: Heap too small (%llu MB), need >= 16MB\n",
		       heap_size / (1024 * 1024));
		return;
	}

	current_pml4_phys = bootstrap_cr3_phys;

	// We reserve 2MB for bootstrap allocator
	uint64_t bootstrap_size = 2 * 1024 * 1024;
	if (bootstrap_size > heap_size / 4)
		bootstrap_size = heap_size / 4;

	vmm_set_bootstrap_allocator(heap_start, bootstrap_size);

	// CRITICAL: Map the entire heap
	printk("Mapping heap into page tables...\n");
	uint64_t heap_pages = (heap_size + PAGE_SIZE - 1) / PAGE_SIZE;

	// We map in blocks of 512 pages for better debugging
	uint64_t mapped = 0;
	const uint64_t chunk = 512;

	while (mapped < heap_pages)
	{
		uint64_t to_map = (heap_pages - mapped) > chunk ? chunk : (heap_pages - mapped);
		uint64_t vaddr = heap_start + mapped * PAGE_SIZE;

		int result = vmm_map(vaddr, vaddr, to_map, PTE_WRITABLE);
		if (result != 0)
		{
			printk("FATAL: Failed to map heap at 0x%llx (error %d)\n",
			       vaddr, result);
			printk("Mapped: %llu/%llu pages\n", mapped, heap_pages);
			return;
		}

		mapped += to_map;

		if (mapped % (1024) == 0) // Progress every 4MB
			;
		printk("  Mapped %llu/%llu pages (%llu MB)\n",
		       mapped, heap_pages,
		       mapped * PAGE_SIZE / (1024 * 1024));
	}

	printk("Successfully mapped %llu pages (%llu MB)\n",
	       heap_pages, heap_pages * PAGE_SIZE / (1024 * 1024));
	printk("=== VMM Init Complete ===\n");
}
