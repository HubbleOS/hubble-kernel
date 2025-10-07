// VMM (Virtual Memory Manager) — the software subsystem of the OS responsible for managing virtual memory.
#include <stdint.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

static uint64_t *pml4;

void vmm_init(void)
{
	pml4 = (uint64_t *)pmm_alloc(PAGE_SIZE);
	memset(pml4, 0, PAGE_SIZE);
}
