#include "vmm.h"
#include "pmm.h"
#include "printk.h"
#include <stdint.h>
#include <string.h>

#define PHYSMAP_BASE 0xFFFF800000000000ULL

static uint64_t current_pml4_phys = 0;
static uint64_t physmap_max = 0;

// Простейший bootstrap allocator для page tables
// Используем конец heap для временных аллокаций
static uint64_t bootstrap_alloc_ptr = 0;
static uint64_t bootstrap_alloc_end = 0;
static int bootstrap_mode = 1;

static uint64_t bootstrap_alloc_page(void)
{
	if (bootstrap_alloc_ptr + PAGE_SIZE > bootstrap_alloc_end)
	{
		printk("ERROR: Bootstrap allocator exhausted!\n");
		return 0;
	}

	uint64_t page = bootstrap_alloc_ptr;
	bootstrap_alloc_ptr += PAGE_SIZE;

	// Очищаем через identity mapping
	memset((void *)page, 0, PAGE_SIZE);

	return page;
}

static uint64_t alloc_table_phys(void)
{
	if (bootstrap_mode)
	{
		// Используем bootstrap allocator
		return bootstrap_alloc_page();
	}
	else
	{
		// Используем PMM
		uint64_t phys = pmm_alloc_phys(1);
		if (!phys)
		{
			printk("ERROR: PMM alloc failed in VMM\n");
			return 0;
		}

		// Очищаем через physmap
		void *virt = (void *)(phys + PHYSMAP_BASE);
		memset(virt, 0, PAGE_SIZE);

		return phys;
	}
}

static inline size_t idx_from_virt(uint64_t virt, int level)
{
	int shift = 12 + (level - 1) * 9;
	return (virt >> shift) & 0x1FF;
}

// Получить указатель на таблицу
static uint64_t *get_table_ptr(uint64_t phys)
{
	// Если physmap создан для этого адреса - используем его
	if (phys < physmap_max)
	{
		return (uint64_t *)(phys + PHYSMAP_BASE);
	}
	// Иначе используем identity mapping
	return (uint64_t *)phys;
}

static uint64_t *get_pte_for(uint64_t pml4_phys, uint64_t virt, int create)
{
	uint64_t phys = pml4_phys;

	for (int level = 4; level > 1; level--)
	{
		uint64_t *table = get_table_ptr(phys);
		size_t idx = idx_from_virt(virt, level);
		uint64_t entry = table[idx];

		if (!(entry & PTE_PRESENT))
		{
			if (!create)
				return NULL;

			uint64_t new_phys = alloc_table_phys();
			if (!new_phys)
				return NULL;

			table[idx] = new_phys | PTE_PRESENT | PTE_WRITABLE;

			// Flush TLB для родительской таблицы
			asm volatile("invlpg (%0)" : : "r"(table) : "memory");

			entry = table[idx];
		}

		phys = entry & 0x000FFFFFFFFFF000ULL;
	}

	uint64_t *pt = get_table_ptr(phys);
	size_t tidx = idx_from_virt(virt, 1);
	return &pt[tidx];
}

int vmm_map(uint64_t virt, uint64_t phys, size_t pages, uint64_t flags)
{
	if (virt & (PAGE_SIZE - 1) || phys & (PAGE_SIZE - 1))
		return -1;

	flags |= PTE_PRESENT;

	for (size_t i = 0; i < pages; i++)
	{
		uint64_t curr_virt = virt + i * PAGE_SIZE;
		uint64_t curr_phys = phys + i * PAGE_SIZE;

		uint64_t *pte = get_pte_for(current_pml4_phys, curr_virt, 1);
		if (!pte)
		{
			printk("ERROR: Failed to get PTE for virt 0x%lx\n", curr_virt);
			return -2;
		}

		*pte = curr_phys | flags;
		asm volatile("invlpg (%0)" : : "r"(curr_virt) : "memory");
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
				pmm_free_phys(phys, 1);
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
		uint64_t *table = get_table_ptr(phys);
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

	uint64_t *pt = get_table_ptr(phys);
	size_t tidx = idx_from_virt(virt, 1);
	uint64_t e = pt[tidx];

	if (!(e & PTE_PRESENT))
		return 0;

	return (e & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

void vmm_init(uint64_t uefi_cr3_phys, uint64_t heap_start, uint64_t heap_size)
{
	printk("=== VMM Init ===\n");

	current_pml4_phys = uefi_cr3_phys;

	// Резервируем последние 4MB heap для bootstrap allocator
	uint64_t bootstrap_size = 4 * 1024 * 1024;
	bootstrap_alloc_end = heap_start + heap_size;
	bootstrap_alloc_ptr = bootstrap_alloc_end - bootstrap_size;

	printk("Bootstrap allocator: 0x%lx - 0x%lx (4 MB)\n",
	       bootstrap_alloc_ptr, bootstrap_alloc_end);

	// Уменьшаем доступный heap (PMM не должен трогать эту область)
	heap_size -= bootstrap_size;

	// Определяем сколько нужно замапить
	uint64_t heap_end = heap_start + heap_size;
	uint64_t max_phys_to_map = heap_end;

	// Ограничиваем разумным пределом
	uint64_t reasonable_limit = 16ULL * 1024 * 1024 * 1024;
	if (max_phys_to_map > reasonable_limit)
	{
		max_phys_to_map = reasonable_limit;
	}

	max_phys_to_map = (max_phys_to_map + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	printk("Creating physmap: 0 - 0x%lx (%lu MB)\n",
	       max_phys_to_map, max_phys_to_map / (1024 * 1024));

	// Мапим небольшими блоками с прогрессом
	uint64_t block_size = 64 * 1024 * 1024; // 64MB
	uint64_t mapped = 0;
	int progress = 0;

	while (mapped < max_phys_to_map)
	{
		uint64_t to_map = block_size;
		if (mapped + to_map > max_phys_to_map)
		{
			to_map = max_phys_to_map - mapped;
		}

		uint64_t vaddr = PHYSMAP_BASE + mapped;
		uint64_t paddr = mapped;
		uint64_t pages = to_map / PAGE_SIZE;

		int ret = vmm_map(vaddr, paddr, pages, PTE_PRESENT | PTE_WRITABLE);
		if (ret != 0)
		{
			printk("ERROR: vmm_map failed at 0x%lx (ret=%d)\n", mapped, ret);
			break;
		}

		mapped += to_map;
		physmap_max = mapped;

		// Показываем прогресс каждые 25%
		int new_progress = (mapped * 100) / max_phys_to_map;
		if (new_progress >= progress + 25)
		{
			printk("  Progress: %d%% (%lu MB / %lu MB)\n",
			       new_progress,
			       mapped / (1024 * 1024),
			       max_phys_to_map / (1024 * 1024));
			progress = new_progress;
		}
	}

	printk("Physmap created: %lu MB (0x%lx - 0x%lx)\n",
	       physmap_max / (1024 * 1024),
	       PHYSMAP_BASE, PHYSMAP_BASE + physmap_max);

	// Выключаем bootstrap mode - теперь можно использовать PMM
	bootstrap_mode = 0;
	printk("Bootstrap allocator disabled, using PMM\n");

	// Тест physmap
	printk("Testing physmap...\n");
	if (heap_start < physmap_max)
	{
		volatile uint64_t *test_ptr = (uint64_t *)(PHYSMAP_BASE + heap_start + 0x1000);
		*test_ptr = 0xDEADBEEF12345678ULL;

		// Проверяем через identity mapping
		volatile uint64_t *test_identity = (uint64_t *)(heap_start + 0x1000);
		if (*test_identity == 0xDEADBEEF12345678ULL)
		{
			printk("  Physmap test: OK (physmap == identity)\n");
		}
		else
		{
			printk("  Physmap test: FAILED (0x%lx != 0x%lx)\n",
			       *test_ptr, *test_identity);
		}
	}

	printk("=== VMM Init Complete ===\n");
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

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
	return vmm_map(virt, phys, 1, flags);
}

uint64_t vmm_alloc_physical_page(void)
{
	return pmm_alloc_phys(1);
}

void vmm_free_physical_page(uint64_t phys)
{
	pmm_free_phys(phys, 1);
}

int vmm_ensure_physmap(uint64_t phys_addr, uint64_t size)
{
	uint64_t needed = phys_addr + size;
	needed = (needed + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	if (needed <= physmap_max)
	{
		return 0;
	}

	printk("Extending physmap: 0x%lx -> 0x%lx\n", physmap_max, needed);

	uint64_t pages = (needed - physmap_max) / PAGE_SIZE;
	uint64_t vaddr = PHYSMAP_BASE + physmap_max;
	uint64_t paddr = physmap_max;

	int ret = vmm_map(vaddr, paddr, pages, PTE_PRESENT | PTE_WRITABLE);
	if (ret == 0)
	{
		physmap_max = needed;
	}

	return ret;
}
