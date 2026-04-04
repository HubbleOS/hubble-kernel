#include <hubble/platform.h>
#include <hubble/printk.h>
#include <hubble/init.h>
#include <hubble/cpu.h>
#include <hubble/memory.h>

#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/smp.h>

#include <io.h>
#include <asm.h>

#include <hubble/device.h>

#include <net/eth.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/udp.h>

#include <sound/core/dev.h>

platform_info_t g_platform;

#include <user/exec.h>

#include <src/console.h>

static const struct
{
	uint32_t freq;
	uint32_t ms;
} melody[] = {
    {660, 100},
    {0, 50},
    {660, 100},
    {0, 100},
    {660, 100},
    {0, 100},
    {510, 100},
    {0, 50},
    {660, 100},
    {0, 100},
    {770, 100},
    {0, 300},
    {380, 100},
    {0, 300},

    {510, 100},
    {0, 150},
    {380, 100},
    {0, 200},
    {320, 100},
    {0, 200},
    {440, 100},
    {0, 100},
    {480, 80},
    {0, 80},
    {450, 100},
    {0, 50},
    {430, 100},
    {0, 50},
    {380, 100},
    {0, 50},
    {660, 80},
    {0, 80},
    {760, 50},
    {0, 50},
    {860, 100},
    {0, 100},
    {700, 80},
    {0, 80},
    {760, 50},
    {0, 50},
    {660, 80},
    {0, 80},
    {520, 80},
    {0, 80},
    {580, 80},
    {0, 80},
    {480, 80},
    {0, 80},
};

#include <hubble/init.h>

extern initcall_t __initcalls_start[];
extern initcall_t __initcalls_end[];

static void do_initcalls_range(initcall_t *start, initcall_t *end)
{
	for (initcall_t *fn = start; fn < end; fn++)
	{
		int ret = (*fn)();
		if (ret != 0)
			printk("[init] initcall %p failed: %d\n", fn, ret);
	}
}

extern initcall_t __start___initcalls_early[];
extern initcall_t __stop___initcalls_early[];

extern initcall_t __start___initcalls_core[];
extern initcall_t __stop___initcalls_core[];

extern initcall_t __start___initcalls_fs[];
extern initcall_t __stop___initcalls_fs[];

extern initcall_t __start___initcalls_device[];
extern initcall_t __stop___initcalls_device[];

extern initcall_t __start___initcalls_late[];
extern initcall_t __stop___initcalls_late[];

void do_initcalls(void)
{
	do_initcalls_range(__start___initcalls_early, __stop___initcalls_early);
	do_initcalls_range(__start___initcalls_core, __stop___initcalls_core);
	do_initcalls_range(__start___initcalls_fs, __stop___initcalls_fs);
	do_initcalls_range(__start___initcalls_device, __stop___initcalls_device);
	do_initcalls_range(__start___initcalls_late, __stop___initcalls_late);
}

BootInfo *g_boot_info;

void test()
{
	printk("hello");
}

#include <higher_half.h>

void start_kernel(void)
{
	g_platform.fb_base = (uint64_t)g_boot_info->framebuffer.base;
	g_platform.fb_width = g_boot_info->framebuffer.width;
	g_platform.fb_height = g_boot_info->framebuffer.height;
	g_platform.fb_pitch = g_boot_info->framebuffer.pitch;

	printk_init(&g_boot_info->framebuffer);
	void *virt = (void *)test;
	void *phys = (void *)virt_to_phys((uint64_t)virt);

	printk("virt: %p\nphys: %p\n", virt, phys);

	boot_cpu_init();
	acpi_init(g_boot_info->rsdp); // parses MADT, learns LAPIC/IOAPIC addresses
	boot_memory_init();
	apic_init_bsp(); // now the LAPIC address is known
	hpet_init();

	smp_init();

	// while (1)
	// {
	// 	/* code */
	// }

	//

	do_initcalls();

	// while (1)
	// {
	// 	/* code */
	// }

	// sound_init();

	// while (1)
	// {
	// 	for (int i = 0; i < (int)(sizeof(melody) / sizeof(melody[0])); i++)
	// 		sound_play(melody[i].freq, melody[i].ms);
	// }

	// if (!device_find_by_type(DEV_NET))
	// {
	// 	printk("[net] no network device\n");
	// }

	// arp_request(ARP_IP(10, 0, 2, 2));

	// uint8_t gw_mac[6];

	// uint8_t buf[1500];
	// uint16_t len;
	// while (arp_lookup(ARP_IP(10, 0, 2, 2), gw_mac) != 0)
	// 	eth_recv(buf, &len, NULL);

	// if (arp_lookup(ARP_IP(10, 0, 2, 2), gw_mac) != 0)
	// {
	// 	printk("[net] ARP failed\n");
	// }
	// else
	// {
	// 	printk("[net] ARP ok, sending UDP\n");

	// 	char msg[] = "Hello from kernel!";

	// 	printk("[net] UDP sent\n");
	// }

	// printk("[net] listening on port 7777...\n");

	// uint8_t rbuf[1500];
	// uint16_t rlen;

	// while (1)
	// {

	// 	while (udp_recv(7777, rbuf, &rlen) != 0)
	// 		;

	// 	rbuf[rlen] = 0;
	// 	printk("[net] received: %s\n", rbuf);
	// }

	scheduler_init();

	while (1)
	{
		hlt();
	}
}

void kmain_thread(void)
{
	printk("kmain thread\n");

	task_t *task1 = exec("/usr/bin/user.elf");
	printk("user at cr3: 0x%016lx\n", task1->page_table);
	scheduler_add_task(task1);

	while (1)
	{
		hlt();
	}
}
