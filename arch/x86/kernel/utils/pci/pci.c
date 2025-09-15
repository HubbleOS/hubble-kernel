
#include <stdint.h>
#include <stdlib.h>

#include "stdio.h"
#include "utils/pci/pci.h"

#include "utils/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// Читання 32-біт з конфігураційного простору PCI
static inline uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) |
			((uint32_t)slot << 11) | ((uint32_t)func << 8) |
			(offset & 0xFC);
	*(volatile uint32_t *)PCI_CONFIG_ADDRESS = addr;
	return *(volatile uint32_t *)PCI_CONFIG_DATA;
}

// Алокація структури pci_device
static struct pci_device *allocate_pci_device_struct(uint8_t bus, uint8_t slot, uint8_t func)
{
	struct pci_device *dev = malloc(sizeof(struct pci_device));
	dev->bus = bus;
	dev->slot = slot;
	dev->func = func;

	uint32_t class_vendor = pci_read_config(bus, slot, func, 0x08); // class code
	dev->class_code = ((class_vendor >> 8) & 0xFFFFFF);

	uint32_t bar0 = pci_read_config(bus, slot, func, 0x10);
	dev->bar0 = bar0 & ~0xF; // відкидаємо флаги
	return dev;
}
uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint32_t address;
	uint32_t lbus = (uint32_t)bus;
	uint32_t lslot = (uint32_t)slot;
	uint32_t lfunc = (uint32_t)func;
	uint16_t tmp = 0;

	// Create configuration address as per Figure 1
	address = (uint32_t)((lbus << 16) | (lslot << 11) |
			     (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

	// Write out the address
	outl(0xCF8, address);
	// Read in the data
	// (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
	tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
	return tmp;
}

// Оптимізований пошук NVMe у QEMU
struct pci_device *find_nvme_qemu()
{
	uint16_t vendor, device;
	/* Try and read the first configuration register. Since there are no
	 * vendors that == 0xFFFF, it must be a non-existent device. */
	if ((vendor = pciConfigReadWord(0, 3, 0, 0)) != 0xFFFF)
	{
		printf("vendor: %x\n", vendor);
		device = pciConfigReadWord(0, 3, 0, 2);
		if (vendor == 0x1AF4 && device == 0x1)
		{

			return allocate_pci_device_struct(0, 3, 0);
		}
	}
	return NULL;
}

// Мапінг BAR0
uint64_t pci_map_bar(struct pci_device *dev)
{
	return dev->bar0;
}
