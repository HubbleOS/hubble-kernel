#include <hubble/kernel.h>
#include <hubble/platform.h>
#include <hubble/printk.h>

#include "init/init.h"

#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/smp.h>
#include <dev/mouse.h>
#include <dev/keyboard.h>
#include <dev/ps2.h>
#include <io.h>
#include <asm.h>

platform_info_t g_platform;

#include <user/exec.h>

extern int elf_load(const char *path, uint64_t *entry_out, uint64_t *pm);

uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return (uint64_t)g_boot_info->framebuffer->base;
};

#include <src/early_console.h>

__attribute__((section(".text.boot"))) void
kernel_entry(BootInfo *bi)
{
	clear_bss();
	relocate_boot_info(bi);
	g_boot_info = bi;

	g_platform.fb_base = (uint64_t)g_boot_info->framebuffer->base;
	g_platform.fb_width = g_boot_info->framebuffer->width;
	g_platform.fb_height = g_boot_info->framebuffer->height;
	g_platform.fb_pitch = g_boot_info->framebuffer->pitch;

	early_printk_init(g_boot_info->framebuffer);

	acpi_init(g_boot_info->rsdp);
	hpet_init();
	init_memory(g_boot_info);
	init_cpu();

	//
	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();

	smp_init();

	kernel_main();

	scheduler_init();

	while (1)
	{
		hlt();
	}
}
void pipe_test(void);
void kmain_thread(void)
{
	printk("kmain thread\n");
	VFS_File *pipe = vfs_open("/pipe/test", VFS_O_RDWR | VFS_O_CREAT);
	if (pipe == NULL)
	{
		printk("failed to open pipe\n");
		while (1)
		{
			hlt();
		}
	}
	task_t *task1 = exec("/usr/bin/user1.elf");
	if (task1 != NULL)
	{

		scheduler_add_task(task1);
	}

	// task_t *task2 = task_create(pipe_test, 0, 0);
	// scheduler_add_task(task2);
	char buf[128];
	while (1)
	{
		vfs_lseek(pipe, 0, SEEK_SET);
		int readed = vfs_read(pipe, &buf, 128);
		buf[readed] = '\0';
		printk("%s", buf);
		hlt();
	}
}

void pipe_test(void)
{
	VFS_File *pipe = vfs_open("/pipe/test", VFS_O_RDWR | VFS_O_CREAT);
	VFS_File *kbd_file = vfs_open("/dev/kbd", VFS_O_RDWR);
	char buf[128];
	int pos = 0;
	while (1)
	{
		char c;
		vfs_read(kbd_file, &c, 1);
		buf[pos++] = c;

		// buf[pos - 1] = '\0';
		vfs_lseek(pipe, 0, SEEK_SET);
		vfs_write(pipe, buf, pos);
		pos = 0;

		// vfs_write(pipe, "hello\n", 6);
		hlt();
	}
}
