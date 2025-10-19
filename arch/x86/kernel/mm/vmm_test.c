#include "vmm.h"
#include "pmm.h"
#include <stdio.h>

#define TEST_VIRT 0x0000000010000000ULL
#define TEST_PAGES 2

void vmm_test(void)
{
	printf("\n=== VMM Test Start ===\n");

	uint64_t phys[TEST_PAGES];
	void *virt_ptrs[TEST_PAGES];

	// 1. Выделяем физические фреймы
	for (int i = 0; i < TEST_PAGES; i++)
	{
		virt_ptrs[i] = pmm_alloc(1);
		if (!virt_ptrs[i])
		{
			printf("Failed to alloc physical frame %d\n", i);
			return;
		}
		phys[i] = pmm_get_phys(virt_ptrs[i]);
		printf("Allocated frame %d: virt=0x%llx phys=0x%llx\n",
		       i, (uint64_t)virt_ptrs[i], phys[i]);
	}

	// 2. Маппим в тестовую область
	printf("\nMapping test region...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		int result = vmm_map(TEST_VIRT + i * PAGE_SIZE, phys[i], 1, PTE_WRITABLE);
		if (result != 0)
		{
			printf("Failed to map page %d (error %d)\n", i, result);
			return;
		}
		printf("Mapped page %d: 0x%llx -> 0x%llx\n",
		       i, TEST_VIRT + i * PAGE_SIZE, phys[i]);
	}

	// 3. Проверяем translation
	printf("\nTesting translation...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		uint64_t vaddr = TEST_VIRT + i * PAGE_SIZE;
		uint64_t translated = vmm_translate(vaddr);
		printf("Virt 0x%llx -> Phys 0x%llx (expected 0x%llx) %s\n",
		       vaddr, translated, phys[i],
		       translated == phys[i] ? "[OK]" : "[FAIL]");
	}

	// 4. Запись через виртуальные адреса
	printf("\nWriting to virtual addresses...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		uint8_t *ptr = (uint8_t *)(TEST_VIRT + i * PAGE_SIZE);
		ptr[0] = 0xAA + i;
		ptr[1] = 0xBB + i;
		ptr[2] = 0xCC + i;
		printf("Wrote to page %d: [0x%02x, 0x%02x, 0x%02x]\n",
		       i, ptr[0], ptr[1], ptr[2]);
	}

	// 5. Чтение
	printf("\nReading from virtual addresses...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		uint8_t *ptr = (uint8_t *)(TEST_VIRT + i * PAGE_SIZE);
		printf("Read from page %d: [0x%02x, 0x%02x, 0x%02x]\n",
		       i, ptr[0], ptr[1], ptr[2]);
	}

	// 6. Unmap
	printf("\nUnmapping test region...\n");
	vmm_unmap(TEST_VIRT, TEST_PAGES);

	// 7. Освобождаем память
	printf("Freeing physical frames...\n");
	for (int i = 0; i < TEST_PAGES; i++)
	{
		pmm_free(virt_ptrs[i], 1);
	}

	printf("=== VMM Test End ===\n\n");
}
