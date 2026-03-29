#include <hubble/kernel.h>
#include <hubble/platform.h>

#include <hubble/printk.h>

#include "init/init.h"

#include <dev/keyboard.h>
#include <dev/mouse.h>
#include <fs/vfs/dev.h>
////

#include <drivers/net/e1000/e1000.h>

#include <net/eth.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/udp.h>

static uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return g_platform.fb_base;
}

void kernel_main(void)
{
	init_filesystems();
	dev_vfs_register("fb0", fb_mmap, NULL);
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file);
	dev_vfs_register("kbd", NULL, kbd_read);

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
}
