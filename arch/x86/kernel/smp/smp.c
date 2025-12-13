
#include "smp.h"
// #include "apic.h"
// #include "acpi.h"
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/kmalloc.h>

#include <apic/apic.h>
#include <acpi/acpi.h>

#include "percpu.h"
#include "higher_half.h"
#include <printk.h>
#include <string.h>

#include "smp.h"

// #include "percpu.h"
// #include "higher_half.h"
// #include <printk.h>
// #include <string.h>
// #include <mm/vmm.h>
// #include <mm/pmm.h>

#define AP_TRAMPOLINE_ADDR 0x8000
#define AP_STACK_SIZE (64 * 1024)			  // 64KB per CPU
#define AP_STACK_PAGES ((AP_STACK_SIZE + 0xFFF) / 0x1000) // 16 pages

extern uint8_t _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_end[];
extern uint8_t _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_start[];

// AP entry point in kernel
void ap_entry(void);

// Global AP synchronization
static volatile bool ap_ready = false;
static spinlock_t smp_lock = SPINLOCK_INIT("smp");

// Allocate stack for AP using VMM/PMM
static void *allocate_ap_stack(void)
{
	// Allocate physical pages for stack
	uint64_t phys = pmm_alloc_pages(AP_STACK_PAGES);
	if (!phys)
	{
		printk("ERROR: Failed to allocate physical pages for AP stack\n");
		return NULL;
	}

	printk("  Allocated %u physical pages at 0x%lx\n", AP_STACK_PAGES, phys);

	uint64_t virt;

	// Check if physical address is below 4GB (covered by bootloader's mapping)
	if (phys < 0x100000000ULL)
	{
		printk("  Physical address below 4GB, using higher-half mapping\n");
		virt = HIGHER_HALF_BASE + phys;

		// Bootloader should have already mapped this with huge pages
		// Just verify and use it
		printk("  Virtual address: 0x%lx (using bootloader mapping)\n", virt);
	}
	else
	{
		printk("  Physical address above 4GB, need explicit mapping\n");

		// Allocate virtual address space
		// Use a dedicated region for AP stacks (adjust to your memory map)
		static uint64_t next_ap_stack_virt = 0xFFFFFF8000000000ULL;
		virt = next_ap_stack_virt;
		next_ap_stack_virt += AP_STACK_SIZE;

		printk("  Virtual address: 0x%lx (explicit mapping)\n", virt);

		// Map each page
		for (size_t i = 0; i < AP_STACK_PAGES; i++)
		{
			uint64_t page_virt = virt + (i * VMM_PAGE_SIZE);
			uint64_t page_phys = phys + (i * VMM_PAGE_SIZE);

			if (vmm_map_page(page_virt, page_phys, VMM_FLAGS_STACK) != 0)
			{
				printk("ERROR: Failed to map AP stack page %zu\n", i);
				pmm_free_pages(phys, AP_STACK_PAGES);
				return NULL;
			}
		}
	}

	// Zero out the stack
	printk("  Zeroing %u bytes at 0x%lx...\n", AP_STACK_SIZE, virt);
	memset((void *)virt, 0, AP_STACK_SIZE);
	printk("  Stack zeroed successfully\n");

	// Return stack top (stacks grow downward)
	void *stack_top = (void *)(virt + AP_STACK_SIZE);
	printk("  Stack: 0x%lx - 0x%lx (top at %p)\n", virt, virt + AP_STACK_SIZE, stack_top);
	return stack_top;
}

// AP kernel entry point (called by trampoline in long mode)
void ap_entry(void)
{
	// Get APIC ID
	uint8_t apic_id = lapic_get_id();

	// Enable Local APIC
	lapic_enable();

	// Initialize per-CPU data
	percpu_init_ap(apic_id);

	// Signal that we're ready
	ap_ready = true;

	printk("AP %u online!\n", apic_id);

	// TODO: Enter scheduler or idle loop
	while (1)
	{
		asm volatile("hlt");
	}
}

// Callback for enumerating CPUs
static void start_ap_callback(uint8_t apic_id, uint8_t processor_id, void *ctx)
{
	uint8_t bsp_id = lapic_get_id();
	if (apic_id == bsp_id)
		return;

	printk("Starting AP: APIC ID %u, Processor ID %u\n", apic_id, processor_id);

	void *stack_top = allocate_ap_stack();
	if (!stack_top)
	{
		printk("ERROR: Failed to allocate stack for AP %u\n", apic_id);
		return;
	}

	size_t trampoline_size =
	    _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_end -
	    _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_start;

	volatile uint64_t *trampoline_stack = (uint64_t *)(PHYS_TO_VIRT(AP_TRAMPOLINE_ADDR + trampoline_size - 16));
	volatile uint64_t *trampoline_entry = (uint64_t *)(PHYS_TO_VIRT(AP_TRAMPOLINE_ADDR + trampoline_size - 8));

	// Write values FIRST
	*trampoline_stack = (uint64_t)stack_top;
	*trampoline_entry = (uint64_t)ap_entry;

	// Ensure writes complete
	asm volatile("mfence" ::: "memory");

	ap_ready = false;

	// NOW start the AP - pass PHYSICAL address (0x8000, not virtual!)
	apic_start_ap(apic_id, AP_TRAMPOLINE_ADDR);

	// Wait with timeout
	uint32_t timeout = 10000000; // Longer timeout
	while (!ap_ready && timeout > 0)
	{
		timeout--;
		asm volatile("pause");
	}

	if (ap_ready)
	{
		printk("  AP %u started successfully\n", apic_id);
	}
	else
	{
		printk("  ERROR: AP %u failed to start\n", apic_id);
	}

	// Delay before next AP
	for (volatile int i = 0; i < 5000000; i++)
		;
}
int smp_init(void)
{
	printk("=== SMP Initialization ===\n");

	// Initialize per-CPU data for BSP
	percpu_init_bsp();

	// Copy AP trampoline to low memory
	printk("Copying AP trampoline to 0x%x\n", AP_TRAMPOLINE_ADDR);

	void *trampoline_dest = (void *)PHYS_TO_VIRT(AP_TRAMPOLINE_ADDR);
	size_t trampoline_size =
	    _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_end -
	    _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_start;

	printk("  Trampoline size: %u bytes\n", trampoline_size);

	if (trampoline_size > 4096)
	{
		printk("ERROR: Trampoline too large (%u bytes)\n", trampoline_size);
		return -1;
	}

	memcpy(trampoline_dest, _binary__home_underrated_projects_kernel_out_x86_build_ap_trampoline_bin_start, trampoline_size);

	// Write PML4 address to trampoline (at offset for ap_cr3)
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	// Find offset of ap_cr3, ap_stack, ap_entry in trampoline
	// These are at the end of the trampoline code
	volatile uint64_t *trampoline_cr3 = (uint64_t *)(PHYS_TO_VIRT(AP_TRAMPOLINE_ADDR + trampoline_size - 24));
	volatile uint64_t *trampoline_stack = (uint64_t *)(PHYS_TO_VIRT(AP_TRAMPOLINE_ADDR + trampoline_size - 16));
	volatile uint64_t *trampoline_entry = (uint64_t *)(PHYS_TO_VIRT(AP_TRAMPOLINE_ADDR + trampoline_size - 8));

	*trampoline_cr3 = cr3;

	printk("  CR3: 0x%lx\n", cr3);
	printk("  Trampoline data offsets: CR3=%p, Stack=%p, Entry=%p\n",
	       trampoline_cr3, trampoline_stack, trampoline_entry);

	// Enumerate and start all APs
	printk("\nStarting Application Processors:\n");
	acpi_enum_lapics(start_ap_callback, NULL);

	printk("\nSMP initialized: %u CPUs online\n", num_cpus_online);
	return 0;
}

uint32_t smp_get_cpu_count(void)
{
	return num_cpus_online;
}

void smp_send_ipi_all(uint8_t vector)
{
	// Get list of all CPUs
	for (int i = 0; i < MAX_CPUS; i++)
	{
		if (cpu_data[i].online && cpu_data[i].apic_id != lapic_get_id())
		{
			lapic_send_ipi(cpu_data[i].apic_id, vector);
		}
	}
}

void smp_call_function_all(void (*func)(void *), void *arg)
{
	// TODO: Implement IPI-based function calls
	// This requires setting up an IPI handler
	printk("smp_call_function_all: not implemented yet\n");
}
