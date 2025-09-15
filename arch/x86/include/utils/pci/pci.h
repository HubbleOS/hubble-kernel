#pragma once

struct pci_device
{
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	uint32_t class_code;
	uint64_t bar0;
};

struct pci_device *find_nvme_qemu();
