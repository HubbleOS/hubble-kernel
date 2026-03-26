#include "e1000.h"
#include <printk.h>
#include <mm/kmalloc.h>
#include <fs/pci/pci.h>
#include <string.h>

#include <higher_half.h>

// Helpers
static volatile uint32_t *e1000_base = NULL;

static inline uint32_t e1000_read(uint32_t reg)
{
	return e1000_base[reg / 4];
}

static inline void e1000_write(uint32_t reg, uint32_t val)
{
	e1000_base[reg / 4] = val;
}

static struct e1000_rx_desc *rx_descs = NULL;
static struct e1000_tx_desc *tx_descs = NULL;
static uint8_t *rx_buffers[E1000_RX_DESC_COUNT];
static uint32_t rx_tail = 0;
static uint32_t tx_tail = 0;

static uint8_t mac_addr[6];

// PCI find e1000
static uint64_t find_e1000_bar0(void)
{
	for (uint16_t bus = 0; bus < 256; bus++)
	{
		for (uint8_t slot = 0; slot < 32; slot++)
		{
			for (uint8_t func = 0; func < 8; func++)
			{
				uint32_t val = pci_read_config(bus, slot, func, 0x00);
				uint16_t vendor = val & 0xFFFF;

				if (vendor == 0xFFFF || vendor == 0x0000)
					continue;

				uint16_t device = (val >> 16) & 0xFFFF;

				printk("[pci] %02x:%02x.%d vendor=%04x device=%04x\n",
				       bus, slot, func, vendor, device);

				if (vendor == E1000_VENDOR_ID &&
				    device == E1000_DEVICE_ID)
				{
					printk("[e1000] FOUND at %02x:%02x.%d\n",
					       bus, slot, func);

					uint32_t bar0 = pci_read_config(bus, slot, func, 0x10);
					return (uint64_t)(bar0 & ~0xFU);
				}
			}
		}
	}
	return 0;
}

static void e1000_rx_init(void)
{
	rx_descs = kmalloc(sizeof(struct e1000_rx_desc) * E1000_RX_DESC_COUNT, GFP_KERNEL);
	memset(rx_descs, 0, sizeof(struct e1000_rx_desc) * E1000_RX_DESC_COUNT);

	for (int i = 0; i < E1000_RX_DESC_COUNT; i++)
	{
		rx_buffers[i] = kmalloc(E1000_BUFFER_SIZE, GFP_KERNEL);
		rx_descs[i].addr = (uint64_t)rx_buffers[i];
		rx_descs[i].status = 0;
	}

	// uint64_t phys = (uint64_t)rx_descs;
	uint64_t phys = VIRT_TO_PHYS(tx_descs);
	e1000_write(E1000_RDBAL, (uint32_t)(phys & 0xFFFFFFFF));
	e1000_write(E1000_RDBAH, (uint32_t)(phys >> 32));
	e1000_write(E1000_RDLEN, E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
	e1000_write(E1000_RDH, 0);
	e1000_write(E1000_RDT, E1000_RX_DESC_COUNT - 1);
	rx_tail = 0;

	e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048);
}

static void e1000_tx_init(void)
{
	tx_descs = kmalloc(sizeof(struct e1000_tx_desc) * E1000_TX_DESC_COUNT, GFP_KERNEL);
	memset(tx_descs, 0, sizeof(struct e1000_tx_desc) * E1000_TX_DESC_COUNT);

	uint64_t phys = (uint64_t)tx_descs;
	e1000_write(E1000_TDBAL, (uint32_t)(phys & 0xFFFFFFFF));
	e1000_write(E1000_TDBAH, (uint32_t)(phys >> 32));
	e1000_write(E1000_TDLEN, E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
	e1000_write(E1000_TDH, 0);
	e1000_write(E1000_TDT, 0);
	tx_tail = 0;

	e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
}

static void e1000_read_mac(void)
{
	uint32_t low = e1000_read(E1000_RAL0);
	uint32_t high = e1000_read(E1000_RAH0);

	mac_addr[0] = (low >> 0) & 0xFF;
	mac_addr[1] = (low >> 8) & 0xFF;
	mac_addr[2] = (low >> 16) & 0xFF;
	mac_addr[3] = (low >> 24) & 0xFF;
	mac_addr[4] = (high >> 0) & 0xFF;
	mac_addr[5] = (high >> 8) & 0xFF;

	printk("[e1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac_addr[0], mac_addr[1], mac_addr[2],
	       mac_addr[3], mac_addr[4], mac_addr[5]);
}

int e1000_init(void)
{
	uint64_t bar0 = find_e1000_bar0();
	if (!bar0)
	{
		printk("[e1000] not found!\n");
		return -1;
	}

	e1000_base = (volatile uint32_t *)bar0;

	// Сброс
	e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
	// Небольшая задержка (спин)
	for (volatile int i = 0; i < 100000; i++)
		;

	// Set link up
	e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU);

	e1000_read_mac();
	e1000_rx_init();
	e1000_tx_init();

	printk("[e1000] init OK\n");
	return 0;
}

void e1000_get_mac(uint8_t out[6])
{
	for (int i = 0; i < 6; i++)
		out[i] = mac_addr[i];
}

int e1000_send(const void *data, uint16_t len)
{
	uint32_t idx = tx_tail % E1000_TX_DESC_COUNT;

	tx_descs[idx].addr = (uint64_t)data;
	tx_descs[idx].length = len;
	tx_descs[idx].cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_RS;
	tx_descs[idx].status = 0;

	tx_tail = (tx_tail + 1) % E1000_TX_DESC_COUNT;
	e1000_write(E1000_TDT, tx_tail);

	// Polling: ждём пока карта не отправит
	while (!(tx_descs[idx].status & E1000_TX_STAT_DD))
		;

	return 0;
}

int e1000_recv(void *buf, uint16_t *len_out)
{
	uint32_t idx = rx_tail % E1000_RX_DESC_COUNT;

	if (!(rx_descs[idx].status & E1000_RX_STAT_DD))
		return -1; // нет пакета

	uint16_t len = rx_descs[idx].length;
	memcpy(buf, rx_buffers[idx], len);
	*len_out = len;

	// Возвращаем дескриптор карте
	rx_descs[idx].status = 0;
	rx_tail = (rx_tail + 1) % E1000_RX_DESC_COUNT;
	e1000_write(E1000_RDT, rx_tail);

	return 0;
}
