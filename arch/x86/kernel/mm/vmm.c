#include "vmm.h"
#include "pmm.h"
#include "printk.h"

#include <stdint.h>
#include <string.h>

static inline void *phys_to_virt(uint64_t phys)
{
	return (void *)(phys + PHYSMAP_BASE);
}

static inline uint64_t virt_to_phys(void *vptr)
{
	return (uint64_t)vptr - PHYSMAP_BASE;
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
	printk("Bootstrap allocator: 0x%lx - 0x%lx (%lu KB)\n",
	       base, base + size, size / 1024);
}

void vmm_disable_bootstrap_allocator(void)
{
	use_bootstrap = 0;
	printk("Bootstrap allocator disabled (used %lu bytes)\n",
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
		// Check before allocation
		if (bootstrap_alloc_offset + PAGE_SIZE > bootstrap_alloc_size)
		{
			printk("ERROR: Bootstrap allocator exhausted! Used: %lu/%lu bytes\n",
			       bootstrap_alloc_offset, bootstrap_alloc_size);
			return UINT64_MAX; // Return invalid value
		}
		uint64_t phys = bootstrap_alloc_base + bootstrap_alloc_offset;
		bootstrap_alloc_offset += PAGE_SIZE;

		// CRITICAL: Use identity mapping for bootstrap region
		// We can't use phys_to_virt() yet because bootstrap isn't mapped
		void *virt = (void *)phys;
		memset(virt, 0, PAGE_SIZE);
		return phys;
	}

	// After disabling bootstrap - use PMM
	void *virt = pmm_alloc(1);
	if (!virt)
		return UINT64_MAX;

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
		// CRITICAL: During bootstrap, use identity mapping
		uint64_t *table;
		if (use_bootstrap)
		{
			// Bootstrap region is identity-mapped, use direct access
			table = (uint64_t *)phys;
		}
		else
		{
			// After bootstrap, use physmap
			table = (uint64_t *)phys_to_virt(phys);
		}

		size_t idx = idx_from_virt(virt, level);
		uint64_t entry = table[idx];

		if (!(entry & PTE_PRESENT))
		{
			if (!create)
				return NULL;

			uint64_t new_phys = alloc_table_phys();
			if (new_phys == UINT64_MAX)
			{
				printk("ERROR: Failed to allocate page table at level %d\n", level);
				return NULL;
			}
			table[idx] = new_phys | PTE_PRESENT | PTE_WRITABLE;
			entry = table[idx];
		}
		phys = entry & 0x000FFFFFFFFFF000ULL;
	}

	// Get final PT (level 1)
	uint64_t *pt;
	if (use_bootstrap)
	{
		pt = (uint64_t *)phys;
	}
	else
	{
		pt = (uint64_t *)phys_to_virt(phys);
	}

	size_t tidx = idx_from_virt(virt, 1);
	return &pt[tidx];
}

int vmm_map(uint64_t virt, uint64_t phys, size_t pages, uint64_t flags)
{
	if (virt & (PAGE_SIZE - 1))
		return -1;
	if (phys & (PAGE_SIZE - 1))
		return -1;

	// Ensure flags contain PTE_PRESENT
	flags |= PTE_PRESENT;

	for (size_t i = 0; i < pages; i++)
	{
		uint64_t curr_virt = virt + i * PAGE_SIZE;
		uint64_t curr_phys = phys + i * PAGE_SIZE;

		uint64_t *pte = get_pte_for(current_pml4_phys, curr_virt, 1);
		if (!pte)
			return -2;

		// Check existing mapping
		if (*pte & PTE_PRESENT)
		{
			uint64_t old_phys = *pte & 0x000FFFFFFFFFF000ULL;
			if (old_phys != curr_phys)
			{
				printk("WARNING: Remapping VA=0x%lx: PA=0x%lx -> PA=0x%lx\n",
				       curr_virt, old_phys, curr_phys);
			}
		}

		*pte = curr_phys | flags;
	}

	// Invalidate TLB
	for (size_t i = 0; i < pages; i++)
	{
		asm volatile("invlpg (%0)" : : "r"(virt + i * PAGE_SIZE) : "memory");
	}

	return 0;
}

int vmm_unmap(uint64_t virt, size_t pages, int free_pages)
{
	if (virt & (PAGE_SIZE - 1))
		return -1;

	for (size_t i = 0; i < pages; i++)
	{
		uint64_t curr_virt = virt + i * PAGE_SIZE;
		uint64_t *pte = get_pte_for(current_pml4_phys, curr_virt, 0);
		if (!pte)
			continue;

		if (*pte & PTE_PRESENT)
		{
			if (free_pages)
			{
				uint64_t phys = *pte & 0x000FFFFFFFFFF000ULL;
				vmm_free_physical_page(phys);
			}
			*pte = 0;
		}

		asm volatile("invlpg (%0)" : : "r"(curr_virt) : "memory");
	}

	return 0;
}

uint64_t vmm_translate(uint64_t virt)
{
	uint64_t phys = current_pml4_phys;

	for (int level = 4; level >= 1; level--)
	{
		uint64_t *table;
		if (use_bootstrap)
		{
			table = (uint64_t *)phys;
		}
		else
		{
			table = (uint64_t *)phys_to_virt(phys);
		}

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

	uint64_t *pt;
	if (use_bootstrap)
	{
		pt = (uint64_t *)phys;
	}
	else
	{
		pt = (uint64_t *)phys_to_virt(phys);
	}

	size_t tidx = idx_from_virt(virt, 1);
	uint64_t e = pt[tidx];

	if (!(e & PTE_PRESENT))
		return 0;

	return (e & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

void vmm_init(uint64_t bootstrap_cr3_phys,
	      uint64_t bootstrap_start, uint64_t bootstrap_size,
	      uint64_t heap_start, uint64_t heap_size)
{
	printk("=== VMM Init ===\n");
	printk("Bootstrap CR3: 0x%lx\n", bootstrap_cr3_phys);
	printk("Bootstrap allocator: 0x%lx - 0x%lx (%lu MB)\n",
	       bootstrap_start, bootstrap_start + bootstrap_size,
	       bootstrap_size / (1024 * 1024));
	printk("Heap: 0x%lx - 0x%lx (%lu MB)\n",
	       heap_start, heap_start + heap_size, heap_size / (1024 * 1024));

	if (!bootstrap_cr3_phys)
	{
		printk("FATAL: No bootstrap CR3 provided\n");
		return;
	}

	// Check minimum sizes
	if (heap_size < 16 * 1024 * 1024)
	{
		printk("FATAL: Heap too small (%lu MB), need >= 16MB\n",
		       heap_size / (1024 * 1024));
		return;
	}

	if (bootstrap_size < 2 * 1024 * 1024)
	{
		printk("FATAL: Bootstrap allocator too small (%lu MB), need >= 2MB\n",
		       bootstrap_size / (1024 * 1024));
		return;
	}

	current_pml4_phys = bootstrap_cr3_phys;

	// Initialize bootstrap allocator with the DEDICATED region
	vmm_set_bootstrap_allocator(bootstrap_start, bootstrap_size);

	// NOTE: We DON'T map bootstrap region first!
	// Bootstrap uses identity mapping (VA = PA) during allocation
	// This is safe because UEFI already identity-mapped low memory

	// Map the heap
	printk("Mapping heap into page tables...\n");
	uint64_t heap_pages = (heap_size + PAGE_SIZE - 1) / PAGE_SIZE;
	printk("  Total pages to map: %lu (%lu MB)\n",
	       heap_pages, heap_pages * PAGE_SIZE / (1024 * 1024));

	uint64_t mapped = 0;
	const uint64_t chunk = 512; // Map in 2MB chunks

	while (mapped < heap_pages)
	{
		uint64_t to_map = (heap_pages - mapped) > chunk ? chunk : (heap_pages - mapped);
		uint64_t vaddr = heap_start + mapped * PAGE_SIZE;

		int result = vmm_map(vaddr, vaddr, to_map, PTE_PRESENT | PTE_WRITABLE);
		if (result != 0)
		{
			printk("FATAL: Failed to map heap at 0x%lx (error %d)\n",
			       vaddr, result);
			printk("  Mapped: %lu/%lu pages (%lu MB)\n",
			       mapped, heap_pages, mapped * PAGE_SIZE / (1024 * 1024));
			printk("  Bootstrap used: %lu/%lu bytes (%lu%%)\n",
			       bootstrap_alloc_offset, bootstrap_alloc_size,
			       (bootstrap_alloc_offset * 100) / bootstrap_alloc_size);
			return;
		}

		mapped += to_map;

		// Progress report every 4MB
		if (mapped % 1024 == 0)
		{
			printk("  Mapped %lu/%lu pages (%lu MB)\n",
			       mapped, heap_pages,
			       mapped * PAGE_SIZE / (1024 * 1024));
		}
	}

	// Verify mapping
	printk("Verifying heap mapping...\n");
	uint64_t test_addr = heap_start;
	uint64_t test_phys = vmm_translate(test_addr);
	if (!test_phys)
	{
		printk("FATAL: Heap mapping verification failed at 0x%lx!\n", test_addr);
		return;
	}
	printk("  Heap start 0x%lx -> 0x%lx (OK)\n", test_addr, test_phys);

	// Test write
	volatile uint64_t *test_ptr = (uint64_t *)heap_start;
	*test_ptr = 0xDEADBEEFCAFEBABE;
	if (*test_ptr == 0xDEADBEEFCAFEBABE)
	{
		printk("  Heap write test: OK\n");
	}
	else
	{
		printk("  Heap write test: FAILED (wrote 0xDEADBEEFCAFEBABE, read 0x%lx)\n",
		       *test_ptr);
	}

	printk("Successfully mapped heap: %lu pages (%lu MB)\n",
	       heap_pages, heap_pages * PAGE_SIZE / (1024 * 1024));
	printk("Bootstrap allocator used: %lu/%lu bytes (%lu%%)\n",
	       bootstrap_alloc_offset, bootstrap_alloc_size,
	       (bootstrap_alloc_offset * 100) / bootstrap_alloc_size);

	printk("=== VMM Init Complete ===\n");
}

static int is_table_empty(uint64_t *table)
{
	for (int i = 0; i < 512; i++)
	{
		if (table[i] & PTE_PRESENT)
			return 0;
	}
	return 1;
}

void vmm_cleanup_empty_tables(uint64_t virt)
{
	uint64_t phys = current_pml4_phys;

	for (int level = 4; level > 1; level--)
	{
		uint64_t *table = (uint64_t *)phys_to_virt(phys);
		size_t idx = idx_from_virt(virt, level);
		uint64_t entry = table[idx];

		if (!(entry & PTE_PRESENT))
			return;

		uint64_t next_phys = entry & 0x000FFFFFFFFFF000ULL;
		uint64_t *next_table = (uint64_t *)phys_to_virt(next_phys);

		if (is_table_empty(next_table))
		{
			table[idx] = 0;
			vmm_free_physical_page(next_phys);
			printk("Freed empty page table at level %d\n", level - 1);
		}
		else
		{
			phys = next_phys;
		}
	}
}

uint64_t vmm_alloc_physical_page(void)
{
	void *page = pmm_alloc(1);
	if (!page)
		return 0;
	return pmm_get_phys(page);
}

void vmm_free_physical_page(uint64_t phys)
{
	void *virt = phys_to_virt(phys);
	pmm_free(virt, 1);
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
	return vmm_map(virt, phys, 1, flags);
}
