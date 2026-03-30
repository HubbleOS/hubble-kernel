#pragma once
#include <stdint.h>

struct pci_device
{
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	uint32_t class_code;
	uint64_t bar0;
};

struct pci_device_id
{
	uint16_t vendor;
	uint16_t device;
};

struct pci_driver
{
	const char *name;
	const struct pci_device_id *id_table;
	int (*probe)(struct pci_device *dev);
};

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
int pci_register_driver(struct pci_driver *drv);
