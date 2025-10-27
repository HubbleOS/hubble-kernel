#include <stdint.h>
#include "printk.h"

// GDT entry
typedef struct
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

// GDT entries
static gdt_entry_t gdt[7]; // 0=null, 1=kernel code, 2=kernel data,
			   // 3=user code, 4=user data, 5=TSS low, 6=TSS high

void gdt_set_gate(int num, uint32_t base, uint32_t limit,
		  uint8_t access, uint8_t gran)
{
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;

	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].granularity = (limit >> 16) & 0x0F;
	gdt[num].granularity |= gran & 0xF0;
	gdt[num].access = access;
}

void gdt_init(void)
{
	printk("Setting up GDT...\n");

	// Null descriptor
	gdt_set_gate(0, 0, 0, 0, 0);

	// Kernel code segment (0x08)
	// Base=0, Limit=0xFFFFFFFF, Access=0x9A (present, ring 0, code, executable, readable)
	// Granularity=0xCF (4KB pages, 64-bit)
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);

	// Kernel data segment (0x10)
	// Base=0, Limit=0xFFFFFFFF, Access=0x92 (present, ring 0, data, writable)
	// Granularity=0xCF (4KB pages, 64-bit)
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

	// User code segment (0x18, with RPL=3 becomes 0x1B)
	// Base=0, Limit=0xFFFFFFFF, Access=0xFA (present, ring 3, code, executable, readable)
	// Granularity=0xAF (4KB pages, 64-bit)
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xAF);

	// User data segment (0x20, with RPL=3 becomes 0x23)
	// Base=0, Limit=0xFFFFFFFF, Access=0xF2 (present, ring 3, data, writable)
	// Granularity=0xCF (4KB pages, 64-bit)
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

	// TSS will be set up separately if needed (entries 5 and 6)

	// Load GDT
	gdt_ptr_t gdt_ptr;
	gdt_ptr.limit = sizeof(gdt) - 1;
	gdt_ptr.base = (uint64_t)&gdt;

	__asm__ volatile("lgdt %0" : : "m"(gdt_ptr));

	// Reload segment registers
	__asm__ volatile(
	    "push $0x08\n"	     // Push kernel CS
	    "lea 1f(%%rip), %%rax\n" // Load address of next instruction
	    "push %%rax\n"	     // Push return address
	    "lretq\n"		     // Far return to reload CS
	    "1:\n"
	    "mov $0x10, %%ax\n" // Kernel data segment
	    "mov %%ax, %%ds\n"
	    "mov %%ax, %%es\n"
	    "mov %%ax, %%fs\n"
	    "mov %%ax, %%gs\n"
	    "mov %%ax, %%ss\n" ::: "rax");

	printk("GDT loaded\n");
}

// CRITICAL: Also need proper exception handlers!
// When user mode code faults, you need to handle it

// Example: Page Fault handler
void page_fault_handler(void)
{
	uint64_t cr2;
	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

	printk("\n!!! PAGE FAULT !!!\n");
	printk("Faulting address: 0x%lx\n", cr2);
	printk("This might be why your kernel reboots!\n");

	// Get error code from stack if you have it
	// Usually pushed by CPU before calling handler

	// For now, halt
	while (1)
		__asm__("hlt");
}

// General Protection Fault handler
void gpf_handler(void)
{
	printk("\n!!! GENERAL PROTECTION FAULT !!!\n");
	printk("This is often caused by:\n");
	printk("- Invalid segment selector\n");
	printk("- Wrong privilege level\n");
	printk("- Invalid GDT/IDT entry\n");

	while (1)
		__asm__("hlt");
}

// Double Fault handler (last resort before triple fault = reboot)
void double_fault_handler(void)
{
	printk("\n!!! DOUBLE FAULT !!!\n");
	printk("System is about to triple fault and reboot!\n");

	while (1)
		__asm__("hlt");
}
