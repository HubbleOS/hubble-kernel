#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/smp.h>
#include "init/init.h"
#include <printk.h>
#include "higher_half.h"
#include <dev/mouse.h>
#include <dev/keyboard.h>
#include <dev/ps2.h>
#include "io.h"

#include <drivers/net/e1000/e1000.h>

#include <net/eth.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/udp.h>
#include <string.h>

#include <fs/vfs/dev.h>

#include <asm.h>

extern int elf_load(const char *path, uint64_t *entry_out);

uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return (uint64_t)g_boot_info->framebuffer->base;
};

__attribute__((section(".text.boot")))
__attribute__((used)) void
kernel_entry(BootInfo *bi)
{
	clear_bss();
	relocate_boot_info(bi);
	g_boot_info = bi;

	early_printk_init(g_boot_info->framebuffer);

	acpi_init(g_boot_info->rsdp);
	hpet_init();
	init_memory(g_boot_info);
	init_cpu();

	init_filesystems();

	dev_vfs_register("fb0", fb_mmap, NULL);

	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file);
	dev_vfs_register("kbd", NULL, kbd_read);

	smp_init();

	e1000_init();
	e1000_netdev_register();
	// 1. ARP — дізнатись MAC gateway
	arp_request(ARP_IP(10, 0, 2, 2));

	// Чекаємо відповідь
	uint8_t gw_mac[6];

	uint8_t buf[1500];
	uint16_t len;
	while (arp_lookup(ARP_IP(10, 0, 2, 2), gw_mac) != 0)
		eth_recv(buf, &len, NULL);

	// 2. Перевіряємо
	if (arp_lookup(ARP_IP(10, 0, 2, 2), gw_mac) != 0)
	{
		printk("[net] ARP failed\n");
	}
	else
	{
		printk("[net] ARP ok, sending UDP\n");

		// 3. UDP на хост порт 4444
		char msg[] = "Hello from kernel!";
		// udp_send(ARP_IP(10, 0, 2, 2), 12345, 4444, msg, sizeof(msg));
		printk("[net] UDP sent\n");
	}

	printk("[net] listening on port 7777...\n");

	uint8_t rbuf[1500];
	uint16_t rlen;

	while (1)
	{

		while (udp_recv(7777, rbuf, &rlen) != 0)
			;

		rbuf[rlen] = 0; // null terminate
		printk("[net] received: %s\n", rbuf);
	}

	// scheduler_init();

	while (1)
	{
		hlt();
	}
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

void kmain_thread(void)
{
	printk("kmain thread\n");

	// task_t *task1 = task_create(render_task, 255);
	// scheduler_add_task(task1);
	uint64_t entry;
	elf_load("/usr/bin/user.elf", &entry);
	task_t *task1 = task_create((void *)entry, 255, 1);
	scheduler_add_task(task1);

	while (1)
	{
		hlt();
	}
}
