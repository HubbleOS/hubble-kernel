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

void init_cpu(void)
{
	printk(KERN_INFO "Initializing CPU subsystems...\n");
	check_nx_support();

	gdt_init();
	idt_init();
	tss_init();
	interrupts_init();
	syscall_init();

	printk(KERN_INFO "CPU initialization complete\n");
}
