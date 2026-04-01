
#include "smp.h"
// #include "apic.h"
// #include "acpi.h"
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/kmalloc.h>

#include <apic/apic.h>
#include <acpi/acpi.h>
#include <gdt/gdt.h>
#include <hpet/hpet.h>

#include "percpu.h"
#include "higher_half.h"
#include <hubble/printk.h>
#include <hubble/string.h>

#include <syscalls/syscall_entry.h>

#include "smp.h"
#include <msr.h>

#include <asm.h>

static size_t g_trampoline_size = 0;
static volatile uint64_t *g_trampoline_cr3 = NULL;
static volatile uint64_t *g_trampoline_stack = NULL;
static volatile uint64_t *g_trampoline_entry = NULL;

#define AP_TRAMPOLINE_ADDR 0x8000
#define AP_STACK_SIZE (64 * 1024)			  // 64KB per CPU
#define AP_STACK_PAGES ((AP_STACK_SIZE + 0xFFF) / 0x1000) // 16 pages

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];
// AP entry point in kernel
void ap_entry(void);

// Global AP synchronization
static volatile bool ap_ready = false;
static spinlock_t smp_lock = SPINLOCK_INIT("smp");

struct ap_startup_data
{
	uint64_t pml4_phys;	    // Offset 0
	uint16_t gdt_limit;	    // Offset 8
	uint64_t gdt_base;	    // Offset 10 (note: misaligned, but packed)
	uint64_t stack_top;	    // Offset 18
	uint64_t entry_point;	    // Offset 26
	volatile uint32_t ap_ready; // Offset 34
} __attribute__((packed));

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
		vmm_map_page(virt, phys, VMM_FLAGS_STACK);
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

void enable_nxe(void)
{
	uint64_t efer;
	asm volatile(
	    "mov $0xC0000080, %%ecx\n"
	    "rdmsr\n"
	    "or $(1 << 11), %%eax\n"
	    "wrmsr\n"
	    : : : "eax", "ecx", "edx");
}

// AP kernel entry point (called by trampoline in long mode)
void ap_entry(void)
{
	apic_init_ap();

	uint8_t apic_id = lapic_get_id();

	// Initialize per-CPU data
	percpu_init_ap(apic_id);

	// Signal that we're ready
	hpet_init();

	idt_load();
	tss_init();
	syscall_init();
	enable_nxe();
	ap_ready = true;

	uint64_t rflags;
	asm volatile("pushfq; pop %0" : "=r"(rflags));
	printk("AP %u: RFLAGS=0x%lx, IF=%d\n",
	       lapic_get_id(), rflags, (rflags >> 9) & 1);
	sti();
	printk("\nAP %u online!\nHello from AP %u \n\n", apic_id, apic_id);
	lapic_timer_init(100);
	while (1)
	{
		hlt();
	}
}

// Callback for enumerating CPUs
static void start_ap_callback(uint8_t apic_id, uint8_t processor_id, void *ctx)
{
	uint8_t bsp_id = lapic_get_id();
	if (apic_id == bsp_id)
		return;

	printk("Starting AP %u", apic_id);

	// Allocate stack for this AP
	void *stack_top = allocate_ap_stack();
	if (!stack_top)
	{
		printk("ERROR: Failed to allocate stack for AP %u\n", apic_id);
		return;
	}
	printk("AP %u stack top: %p\n", apic_id, stack_top);

	// Get pointer to data structure at end of trampoline
	volatile struct ap_startup_data *data =
	    (volatile struct ap_startup_data *)(AP_TRAMPOLINE_ADDR + 512);

	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	// Fill in the data structure
	data->pml4_phys = cr3;

	data->gdt_limit = get_gdt_limit();
	data->gdt_base = (uint64_t)get_gdt_base();

	data->stack_top = VIRT_TO_PHYS((uint64_t)stack_top) + 0x1000;
	data->entry_point = (uint64_t)ap_entry;

	data->ap_ready = 0;

	// Ensure writes are visible
	asm volatile("mfence" ::: "memory");

	printk("Data structure setup:\n");
	printk("  pml4_phys: 0x%lx\n", data->pml4_phys);
	printk("  gdt_limit: 0x%x\n", data->gdt_limit);
	printk("  gdt_base: 0x%lx\n", data->gdt_base);
	printk("  stack_top: 0x%lx\n", data->stack_top);
	printk("  entry_point: 0x%lx\n", data->entry_point);

	printk("Starting AP %u...\n", apic_id);
	apic_start_ap(apic_id, AP_TRAMPOLINE_ADDR);

	for (volatile int i = 0; i < 10000000; i++)
		;

	volatile uint32_t *marker = (volatile uint32_t *)(AP_TRAMPOLINE_ADDR +
							  offsetof(struct ap_startup_data, ap_ready));

	printk("AP marker: 0x%x\n", *marker);

	// Wait with timeout
	printk("Waiting for AP %u to signal ready...\n", apic_id);
	uint64_t timeout = 1000000000; // Use uint64_t to avoid overflow

	while (data->ap_ready == 0 && timeout > 0) //  Check data->ap_ready, not global
	{
		timeout--;

		if (timeout % 100000000 == 0) // Print every 100M iterations
		{
			printk("  Still waiting... (ap_ready=%u)\n", data->ap_ready);
		}

		asm volatile("pause" ::: "memory"); // Add memory clobber
	}

	if (data->ap_ready)
	{
		printk("AP %u started successfully!\n", apic_id);
	}
	else
	{
		printk("✗ ERROR: AP %u failed to start (timeout)\n", apic_id);
		// Debug: Check if AP modified anything
		printk("  Final ap_ready value: %u\n", data->ap_ready);
	}

	// Small delay before next AP
	for (volatile int i = 0; i < 10000000; i++)
		;
}

// Initialize SMP
int smp_init(void)
{
	printk("SMP Initialization");

	percpu_init_bsp();

	// CRITICAL: The trampoline must be accessible at BOTH:
	// 1. Physical 0x8000 (for AP in real mode)
	// 2. Virtual address for kernel to write to it

	printk("Setting up AP trampoline at 0x%x\n", AP_TRAMPOLINE_ADDR);

	// Method 1: Use identity mapping (0x8000 -> 0x8000)
	// This ensures the AP can access it in real mode
	void *trampoline_dest = (void *)AP_TRAMPOLINE_ADDR;

	printk("  Using identity mapping: virt 0x%lx = phys 0x%x\n",
	       (uint64_t)trampoline_dest, AP_TRAMPOLINE_ADDR);

	// Ensure identity mapping exists
	printk("  Creating identity mapping for trampoline...\n");

	g_trampoline_size =
	    ap_trampoline_end - ap_trampoline_start;

	printk("  Trampoline size: %u bytes (0x%x)\n", g_trampoline_size, g_trampoline_size);

	if (g_trampoline_size > 4096)
	{
		printk("ERROR: Trampoline too large (%u bytes)\n", g_trampoline_size);
		return -1;
	}

	// Copy trampoline to identity-mapped location
	printk("  Copying trampoline code...\n");
	memcpy(trampoline_dest,
	       ap_trampoline_start,
	       g_trampoline_size);

	// Verify the copy worked
	uint8_t *verify = (uint8_t *)trampoline_dest;
	printk("  First bytes at 0x%lx: %02x %02x %02x %02x\n",
	       (uint64_t)verify, verify[0], verify[1], verify[2], verify[3]);

	printk("Trampoline initialized\n");

	// Enumerate and start all APs
	printk("\nStarting Application Processors:\n");
	acpi_enum_lapics(start_ap_callback, NULL);

	printk("SMP Initialization Complete");
	printk("Total CPUs online: %u\n", num_cpus_online);
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
