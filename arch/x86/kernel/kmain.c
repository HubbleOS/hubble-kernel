#include <bootinfo/bootinfo.h>
#include "init/init.h"
#include <printk.h>
#include "higher_half.h"

// Forward declarations
extern void os_main(BootInfo *bi);
extern void syscall_init(void);
extern int load_elf_and_run(const char *path);

// -----------------------------------------------------------------------------
// Kernel entry
// -----------------------------------------------------------------------------

__attribute__((section(".text.boot")))
__attribute__((used)) void
kernel_entry(BootInfo *bi)
{
	clear_bss();
	relocate_boot_info(bi);

	early_printk_init(bi->framebuffer);

	printk(KERN_INFO "=== Higher-Half Kernel Starting ===\n");

	init.cpu();
	init.memory(bi);
	init.filesystems();

	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");

	// printk(KERN_INFO "Starting OS main loop...\n");
	// os_main(bi);

	load_elf_and_run("/usr/bin/user.elf");

	printk(KERN_WARNING "os_main() returned! Entering infinite loop...\n");
	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
