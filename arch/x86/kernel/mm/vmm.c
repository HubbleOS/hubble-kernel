#include "vmm.h"
#include "pmm.h"
#include <string.h>

// Поточний адресний простір
static address_space_t *g_current_as = NULL;

// Kernel адресний простір
static address_space_t g_kernel_as = {0};

// Статистика
static struct
{
	uint64_t mapped_pages;
	uint64_t page_faults;
	uint64_t tlb_flushes;
} g_vmm_stats = {0};

// === Helper Functions ===

// Отримує PTE через рекурсивний мапінг
static inline pte_t *vmm_get_pte(uint64_t virt)
{
	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	uint64_t pd_idx = PD_INDEX(virt);
	uint64_t pt_idx = PT_INDEX(virt);

	page_table_t *pt = (page_table_t *)PT_VADDR(pml4_idx, pdpt_idx, pd_idx);
	return &pt->entries[pt_idx];
}

// Отримує або створює page table
static page_table_t *vmm_get_or_create_table(page_table_t *parent, uint64_t index, uint64_t flags)
{
	pte_t *entry = &parent->entries[index];

	// Якщо таблиця вже існує
	if (*entry & PAGE_PRESENT)
	{
		return (page_table_t *)(*entry & PTE_ADDR_MASK);
	}

	// Створюємо нову таблицю
	uint64_t phys = pmm_alloc_page();
	if (!phys)
	{
		return NULL;
	}

	// Очищуємо таблицю
	page_table_t *table = (page_table_t *)phys;
	memset(table, 0, PAGE_SIZE_4K);

	// Встановлюємо entry
	*entry = phys | flags | PAGE_PRESENT | PAGE_WRITE;

	return table;
}

// Створює рекурсивний мапінг для PML4
static void vmm_setup_recursive_mapping(page_table_t *pml4)
{
	uint64_t pml4_phys = (uint64_t)pml4;
	pml4->entries[RECURSIVE_INDEX] = pml4_phys | PAGE_PRESENT | PAGE_WRITE;
}

// === Public API ===

// void vmm_init(void)
void vmm_init(uint64_t bootloader_pml4_phys)
{
	// Виділяємо PML4 для kernel
	// uint64_t pml4_phys = pmm_alloc_page();
	uint64_t pml4_phys = bootloader_pml4_phys;
	if (!pml4_phys)
	{
		// Критична помилка
		return;
	}

	page_table_t *pml4 = (page_table_t *)pml4_phys;
	// memset(pml4, 0, PAGE_SIZE_4K);

	// Налаштовуємо рекурсивний мапінг
	vmm_setup_recursive_mapping(pml4);

	// Ініціалізуємо kernel address space
	g_kernel_as.pml4_phys = pml4_phys;
	g_kernel_as.pml4_virt = pml4;
	g_kernel_as.heap_start = 0;
	g_kernel_as.heap_end = 0;
	g_kernel_as.stack_start = 0;
	g_kernel_as.stack_end = 0;

	g_current_as = &g_kernel_as;

	// Перемикаємося на нову PML4
	vmm_set_cr3(pml4_phys);

	// Тепер ми можемо створювати мапінги через рекурсивний доступ
	// Identity map перші 4GB для kernel (опціонально, залежить від твоєї архітектури)
	// vmm_map_range(0, 0, 0x100000000, PAGE_KERNEL);
}

address_space_t *vmm_create_address_space(void)
{
	// Виділяємо структуру address_space
	address_space_t *as = (address_space_t *)kmalloc(sizeof(address_space_t));
	if (!as)
	{
		return NULL;
	}

	// Виділяємо PML4
	uint64_t pml4_phys = pmm_alloc_page();
	if (!pml4_phys)
	{
		kfree(as);
		return NULL;
	}

	// Тимчасово мапимо PML4 для ініціалізації
	page_table_t *pml4 = (page_table_t *)pml4_phys;
	memset(pml4, 0, PAGE_SIZE_4K);

	// Налаштовуємо рекурсивний мапінг
	vmm_setup_recursive_mapping(pml4);

	// Копіюємо kernel мапінги (верхня половина адресного простору)
	page_table_t *kernel_pml4 = (page_table_t *)g_kernel_as.pml4_phys;
	for (int i = 256; i < 512; i++)
	{
		pml4->entries[i] = kernel_pml4->entries[i];
	}

	as->pml4_phys = pml4_phys;
	as->pml4_virt = pml4;
	as->heap_start = 0x0000000000400000; // 4MB
	as->heap_end = 0x0000000000400000;
	as->stack_start = 0x00007FFFFFFFF000; // Top of user space
	as->stack_end = 0x00007FFFFFFFF000;

	return as;
}

void vmm_destroy_address_space(address_space_t *as)
{
	if (!as || as == &g_kernel_as)
	{
		return;
	}

	// Тут треба пройтись по всіх page tables та звільнити їх
	// Для простоти зараз просто звільняємо PML4
	// У production версії треба рекурсивно звільнити всі таблиці

	pmm_free_page(as->pml4_phys);
	kfree(as);
}

void vmm_switch_address_space(address_space_t *as)
{
	if (!as)
	{
		return;
	}

	g_current_as = as;
	vmm_set_cr3(as->pml4_phys);
	g_vmm_stats.tlb_flushes++;
}

address_space_t *vmm_get_current_address_space(void)
{
	return g_current_as;
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
	// Вирівнюємо адреси
	virt &= ~0xFFFULL;
	phys &= ~0xFFFULL;

	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	uint64_t pd_idx = PD_INDEX(virt);
	uint64_t pt_idx = PT_INDEX(virt);

	// Отримуємо PML4 через рекурсивний мапінг
	page_table_t *pml4 = (page_table_t *)PML4_VADDR;

	// Отримуємо або створюємо PDPT
	page_table_t *pdpt;
	if (!(pml4->entries[pml4_idx] & PAGE_PRESENT))
	{
		uint64_t pdpt_phys = pmm_alloc_page();
		if (!pdpt_phys)
			return false;

		pdpt = (page_table_t *)pdpt_phys;
		memset(pdpt, 0, PAGE_SIZE_4K);
		pml4->entries[pml4_idx] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
	}
	else
	{
		pdpt = (page_table_t *)PDPT_VADDR(pml4_idx);
	}

	// Отримуємо або створюємо PD
	page_table_t *pd;
	if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT))
	{
		uint64_t pd_phys = pmm_alloc_page();
		if (!pd_phys)
			return false;

		pd = (page_table_t *)pd_phys;
		memset(pd, 0, PAGE_SIZE_4K);
		pdpt->entries[pdpt_idx] = pd_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
	}
	else
	{
		pd = (page_table_t *)PD_VADDR(pml4_idx, pdpt_idx);
	}

	// Отримуємо або створюємо PT
	page_table_t *pt;
	if (!(pd->entries[pd_idx] & PAGE_PRESENT))
	{
		uint64_t pt_phys = pmm_alloc_page();
		if (!pt_phys)
			return false;

		pt = (page_table_t *)pt_phys;
		memset(pt, 0, PAGE_SIZE_4K);
		pd->entries[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
	}
	else
	{
		pt = (page_table_t *)PT_VADDR(pml4_idx, pdpt_idx, pd_idx);
	}

	// Встановлюємо PTE
	pt->entries[pt_idx] = phys | flags | PAGE_PRESENT;

	// Інвалідуємо TLB
	vmm_invlpg(virt);

	g_vmm_stats.mapped_pages++;

	return true;
}

void vmm_unmap_page(uint64_t virt)
{
	virt &= ~0xFFFULL;

	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	uint64_t pd_idx = PD_INDEX(virt);
	uint64_t pt_idx = PT_INDEX(virt);

	page_table_t *pml4 = (page_table_t *)PML4_VADDR;

	if (!(pml4->entries[pml4_idx] & PAGE_PRESENT))
		return;

	page_table_t *pdpt = (page_table_t *)PDPT_VADDR(pml4_idx);
	if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT))
		return;

	page_table_t *pd = (page_table_t *)PD_VADDR(pml4_idx, pdpt_idx);
	if (!(pd->entries[pd_idx] & PAGE_PRESENT))
		return;

	page_table_t *pt = (page_table_t *)PT_VADDR(pml4_idx, pdpt_idx, pd_idx);

	// Очищуємо PTE
	pt->entries[pt_idx] = 0;

	// Інвалідуємо TLB
	vmm_invlpg(virt);

	g_vmm_stats.mapped_pages--;
}

uint64_t vmm_virt_to_phys(uint64_t virt)
{
	virt &= ~0xFFFULL;

	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	uint64_t pd_idx = PD_INDEX(virt);
	uint64_t pt_idx = PT_INDEX(virt);

	page_table_t *pml4 = (page_table_t *)PML4_VADDR;

	if (!(pml4->entries[pml4_idx] & PAGE_PRESENT))
		return 0;

	page_table_t *pdpt = (page_table_t *)PDPT_VADDR(pml4_idx);
	if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT))
		return 0;

	page_table_t *pd = (page_table_t *)PD_VADDR(pml4_idx, pdpt_idx);
	if (!(pd->entries[pd_idx] & PAGE_PRESENT))
		return 0;

	page_table_t *pt = (page_table_t *)PT_VADDR(pml4_idx, pdpt_idx, pd_idx);

	if (!(pt->entries[pt_idx] & PAGE_PRESENT))
		return 0;

	return pt->entries[pt_idx] & PTE_ADDR_MASK;
}

bool vmm_map_range(uint64_t virt_start, uint64_t phys_start, uint64_t size, uint64_t flags)
{
	uint64_t virt = virt_start & ~0xFFFULL;
	uint64_t phys = phys_start & ~0xFFFULL;
	uint64_t end = (virt_start + size + 0xFFF) & ~0xFFFULL;

	while (virt < end)
	{
		if (!vmm_map_page(virt, phys, flags))
		{
			return false;
		}
		virt += PAGE_SIZE_4K;
		phys += PAGE_SIZE_4K;
	}

	return true;
}

void vmm_unmap_range(uint64_t virt_start, uint64_t size)
{
	uint64_t virt = virt_start & ~0xFFFULL;
	uint64_t end = (virt_start + size + 0xFFF) & ~0xFFFULL;

	while (virt < end)
	{
		vmm_unmap_page(virt);
		virt += PAGE_SIZE_4K;
	}
}

uint64_t vmm_alloc(uint64_t size, uint64_t flags)
{
	// Простий лінійний allocator для kernel space
	// У production версії треба використовувати більш розумний алгоритм

	static uint64_t next_virt = 0xFFFF800000000000ULL; // Kernel space start

	uint64_t pages = (size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
	uint64_t virt = next_virt;

	// Виділяємо фізичні сторінки та мапимо їх
	for (uint64_t i = 0; i < pages; i++)
	{
		uint64_t phys = pmm_alloc_page();
		if (!phys)
		{
			// Відкат при помилці
			vmm_unmap_range(virt, i * PAGE_SIZE_4K);
			return 0;
		}

		if (!vmm_map_page(virt + i * PAGE_SIZE_4K, phys, flags))
		{
			pmm_free_page(phys);
			vmm_unmap_range(virt, i * PAGE_SIZE_4K);
			return 0;
		}
	}

	next_virt += pages * PAGE_SIZE_4K;

	return virt;
}

void vmm_free(uint64_t virt, uint64_t size)
{
	uint64_t pages = (size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;

	// Звільняємо фізичні сторінки
	for (uint64_t i = 0; i < pages; i++)
	{
		uint64_t phys = vmm_virt_to_phys(virt + i * PAGE_SIZE_4K);
		if (phys)
		{
			pmm_free_page(phys);
		}
	}

	// Анмапимо віртуальні сторінки
	vmm_unmap_range(virt, size);
}

bool vmm_is_mapped(uint64_t virt)
{
	return vmm_virt_to_phys(virt) != 0;
}

bool vmm_set_flags(uint64_t virt, uint64_t flags)
{
	uint64_t phys = vmm_virt_to_phys(virt);
	if (!phys)
	{
		return false;
	}

	vmm_unmap_page(virt);
	return vmm_map_page(virt, phys, flags);
}

uint64_t vmm_get_flags(uint64_t virt)
{
	virt &= ~0xFFFULL;

	uint64_t pml4_idx = PML4_INDEX(virt);
	uint64_t pdpt_idx = PDPT_INDEX(virt);
	uint64_t pd_idx = PD_INDEX(virt);
	uint64_t pt_idx = PT_INDEX(virt);

	page_table_t *pml4 = (page_table_t *)PML4_VADDR;

	if (!(pml4->entries[pml4_idx] & PAGE_PRESENT))
		return 0;

	page_table_t *pdpt = (page_table_t *)PDPT_VADDR(pml4_idx);
	if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT))
		return 0;

	page_table_t *pd = (page_table_t *)PD_VADDR(pml4_idx, pdpt_idx);
	if (!(pd->entries[pd_idx] & PAGE_PRESENT))
		return 0;

	page_table_t *pt = (page_table_t *)PT_VADDR(pml4_idx, pdpt_idx, pd_idx);

	return pt->entries[pt_idx] & PTE_FLAGS_MASK;
}

void vmm_print_stats(void)
{
	// kprintf("=== VMM Statistics ===\n");
	// kprintf("Mapped pages: %llu\n", g_vmm_stats.mapped_pages);
	// kprintf("Page faults: %llu\n", g_vmm_stats.page_faults);
	// kprintf("TLB flushes: %llu\n", g_vmm_stats.tlb_flushes);
	// kprintf("Current CR3: 0x%llx\n", vmm_get_cr3());
}
