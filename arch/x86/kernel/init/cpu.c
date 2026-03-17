#include "init.h"
#include <printk.h>

#include <gdt/gdt.h>
#include <interrupt/interrupt.h>
#include <syscalls/syscall_entry.h>

static inline void check_nx_support(void)
{
	uint32_t eax, ebx, ecx, edx;

	// CPUID function 0x80000001
	asm volatile(
	    "mov $0x80000001, %%eax\n"
	    "cpuid"
	    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));

	if (edx & (1 << 20))
	{
		printk("[CPU] NX bit supported\n");

		// Enable NX bit in EFER
		uint64_t efer;
		asm volatile("rdmsr" : "=A"(efer) : "c"(0xC0000080));
		efer |= (1 << 11); // Set NXE bit
		asm volatile("wrmsr" ::"A"(efer), "c"(0xC0000080));
		printk("[CPU] NX bit enabled\n");
	}
	else
	{
		printk("[CPU] WARNING: NX bit not supported!\n");
	}
}

static void enable_sse(void)
{
	uint64_t cr0, cr4;

	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(1ULL << 2);
	cr0 |= (1ULL << 1);
	asm volatile("mov %0, %%cr0" ::"r"(cr0));

	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1ULL << 9);
	cr4 |= (1ULL << 10);
	asm volatile("mov %0, %%cr4" ::"r"(cr4));

	// Проверяем что реально записалось
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	printk("[CPU] CR4 after SSE init: 0x%llx, OSFXSR=%d OSXMMEXCPT=%d\n",
	       cr4, !!(cr4 & (1 << 9)), !!(cr4 & (1 << 10)));
}

void init_cpu(void)
{
	printk(KERN_INFO "Initializing CPU subsystems...\n");
	check_nx_support();
	enable_sse();

	gdt_init();
	idt_init();
	tss_init();
	interrupts_init();
	syscall_init();

	printk(KERN_INFO "CPU initialization complete\n");
}
