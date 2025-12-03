#include "init.h"
#include <printk.h>

void init_cpu(void)
{
	printk(KERN_INFO "Initializing CPU subsystems...\n");
	gdt_init();
	idt_init();
	tss_init();
	interrupts_init();
	syscall_init();
	printk(KERN_INFO "CPU initialization complete\n");
}
