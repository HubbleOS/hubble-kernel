#include <hubble/kernel.h>
#include <hubble/platform.h>

#include <hubble/printk.h>

#include "init/init.h"

#include <dev/keyboard.h>
#include <dev/mouse.h>
#include <fs/vfs/dev.h>

#include <drivers/net/e1000/e1000.h>

#include <net/eth.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/udp.h>

#include <sound/sounddev.h>

static uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return g_platform.fb_base;
}

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

void kernel_main(void)
{
	init_filesystems();
	dev_vfs_register("fb0", fb_mmap, NULL);
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file);
	dev_vfs_register("kbd", NULL, kbd_read);

	sound_init();

	while (1)
	{
		for (int i = 0; i < (int)(sizeof(melody) / sizeof(melody[0])); i++)
			sound_play(melody[i].freq, melody[i].ms);
	}

	return;

	e1000_init();
	e1000_netdev_register();

	arp_request(ARP_IP(10, 0, 2, 2));

	uint8_t gw_mac[6];

	uint8_t buf[1500];
	uint16_t len;
	while (arp_lookup(ARP_IP(10, 0, 2, 2), gw_mac) != 0)
		eth_recv(buf, &len, NULL);

	if (arp_lookup(ARP_IP(10, 0, 2, 2), gw_mac) != 0)
	{
		printk("[net] ARP failed\n");
	}
	else
	{
		printk("[net] ARP ok, sending UDP\n");

		char msg[] = "Hello from kernel!";

		printk("[net] UDP sent\n");
	}

	printk("[net] listening on port 7777...\n");

	uint8_t rbuf[1500];
	uint16_t rlen;

	while (1)
	{

		while (udp_recv(7777, rbuf, &rlen) != 0)
			;

		rbuf[rlen] = 0;
		printk("[net] received: %s\n", rbuf);
	}
}
