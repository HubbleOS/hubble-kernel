#include "vmm.h"
#include "pmm.h"
#include "printk.h"

#define TEST_VIRT 0x0000800000000000ULL
#define TEST_PAGES 2

void vmm_test(void)
{
	printk("\n=== VMM Test Start ===\n");

	printk("Checking if TEST_VIRT is already mapped...\n");
	uint64_t existing = vmm_translate(TEST_VIRT);
	if (existing)
	{
		printk("WARNING: 0x%llx already mapped to 0x%llx\n",
		       TEST_VIRT, existing);
		printk("This is likely UEFI identity mapping. Using different address.\n");
	}
	else
	{
		printk("0x%llx is not mapped, good!\n", TEST_VIRT);
	}

	uint64_t phys[TEST_PAGES];
	void *virt_ptrs[TEST_PAGES];

	// 1. Выделяем физические фреймы
	for (int i = 0; i < TEST_PAGES; i++)
	{
		virt_ptrs[i] = pmm_alloc(1);
		if (!virt_ptrs[i])
		{
			printk("Failed to alloc physical frame %d\n", i);
			return;
		}
		phys[i] = pmm_get_phys(virt_ptrs[i]);
		printk("Allocated frame %d: virt=0x%llx phys=0x%llx\n",
		       i, (uint64_t)virt_ptrs[i], phys[i]);
	}

	// 2. Маппим в тестовую область
	printk("\nMapping test region...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		int result = vmm_map(TEST_VIRT + i * PAGE_SIZE, phys[i], 1, PTE_WRITABLE);
		if (result != 0)
		{
			printk("Failed to map page %d (error %d)\n", i, result);
			return;
		}
		printk("Mapped page %d: 0x%llx -> 0x%llx\n",
		       i, TEST_VIRT + i * PAGE_SIZE, phys[i]);
	}

	// 3. Проверяем translation
	printk("\nTesting translation...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		uint64_t vaddr = TEST_VIRT + i * PAGE_SIZE;
		uint64_t translated = vmm_translate(vaddr);
		printk("Virt 0x%llx -> Phys 0x%llx (expected 0x%llx) %s\n",
		       vaddr, translated, phys[i],
		       translated == phys[i] ? "[OK]" : "[FAIL]");
	}

	// 4. Запись через виртуальные адреса
	printk("\nWriting to virtual addresses...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		uint8_t *ptr = (uint8_t *)(TEST_VIRT + i * PAGE_SIZE);
		ptr[0] = 0xAA + i;
		ptr[1] = 0xBB + i;
		ptr[2] = 0xCC + i;
		printk("Wrote to page %d: [0x%02x, 0x%02x, 0x%02x]\n",
		       i, ptr[0], ptr[1], ptr[2]);
	}

	// 5. Чтение
	printk("\nReading from virtual addresses...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		uint8_t *ptr = (uint8_t *)(TEST_VIRT + i * PAGE_SIZE);
		printk("Read from page %d: [0x%02x, 0x%02x, 0x%02x]\n",
		       i, ptr[0], ptr[1], ptr[2]);
	}

	// 6. Unmap
	printk("\nUnmapping test region...\n");
	vmm_unmap(TEST_VIRT, TEST_PAGES);

	// 7. Освобождаем память
	printk("Freeing physical frames...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		pmm_free(virt_ptrs[i], 1);
	}

	printk("=== VMM Test End ===\n\n");
}
