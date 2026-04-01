#pragma once
#include <stdint.h>

// Standard PCI Configuration Space Offsets

#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02

#define PCI_COMMAND 0x04
#define PCI_STATUS 0x06

#define PCI_REVISION_ID 0x08
#define PCI_PROG_IF 0x09
#define PCI_SUBCLASS 0x0A
#define PCI_CLASS_CODE 0x0B

#define PCI_CACHE_LINE_SIZE 0x0C
#define PCI_LATENCY_TIMER 0x0D
#define PCI_HEADER_TYPE 0x0E
#define PCI_BIST 0x0F

#define PCI_BAR0 0x10
#define PCI_BAR1 0x14
#define PCI_BAR2 0x18
#define PCI_BAR3 0x1C
#define PCI_BAR4 0x20
#define PCI_BAR5 0x24

#define PCI_CARDBUS_CIS 0x28

#define PCI_SUBSYSTEM_VENDOR_ID 0x2C
#define PCI_SUBSYSTEM_ID 0x2E

#define PCI_ROM_ADDRESS 0x30

#define PCI_CAPABILITIES_PTR 0x34

#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN 0x3D
#define PCI_MIN_GRANT 0x3E
#define PCI_MAX_LATENCY 0x3F

#define PCI_GET_VENDOR(x) ((x) & 0xFFFF)
#define PCI_GET_DEVICE(x) (((x) >> 16) & 0xFFFF)

#define PCI_GET_CLASS(x) (((x) >> 24) & 0xFF)
#define PCI_GET_SUBCLASS(x) (((x) >> 16) & 0xFF)
#define PCI_GET_PROGIF(x) (((x) >> 8) & 0xFF)

// BAR helpers
#define PCI_BAR_IO 0x1
#define PCI_BAR_MEM_TYPE 0x6
#define PCI_BAR_MEM_64 0x4

#define PCI_BAR_ADDR_MASK (~0xF)
#define PCI_BAR_IO_MASK (~0x3)

// Command Register
#define PCI_COMMAND_IO (1 << 0)		// I/O Space
#define PCI_COMMAND_MEM (1 << 1)	// Memory Space
#define PCI_COMMAND_MASTER (1 << 2)	// Bus Master
#define PCI_COMMAND_SPECIAL (1 << 3)	// Special Cycles
#define PCI_COMMAND_INVALIDATE (1 << 4) // Memory Write and Invalidate
#define PCI_COMMAND_VGA_SNOOP (1 << 5)	// VGA Palette Snoop
#define PCI_COMMAND_PARITY (1 << 6)	// Parity Error Response
/* bit 7 reserved */
#define PCI_COMMAND_SERR (1 << 8)	  // SERR# Enable
#define PCI_COMMAND_FAST_BACK (1 << 9)	  // Fast Back-to-Back
#define PCI_COMMAND_INT_DISABLE (1 << 10) // Interrupt Disable
/* bits 11–15 reserved */

/**
 * @brief Representation of a PCI device in the system
 */
struct pci_device
{
	uint8_t bus;	     /**< PCI bus number */
	uint8_t slot;	     /**< Slot number on the bus */
	uint8_t func;	     /**< Function number (0-7) */
	uint32_t class_code; /**< Device class code (Class/Subclass/ProgIF) */
	uint64_t bar0;	     /**< First Base Address Register (BAR0) */
};

/**
 * @brief Structure for PCI device IDs supported by a driver
 */
struct pci_device_id
{
	uint16_t vendor; /**< Vendor ID of the device */
	uint16_t device; /**< Device ID */
};

/**
 * @brief PCI driver structure
 */
struct pci_driver
{
	const char *name;		      /**< Driver name */
	const struct pci_device_id *id_table; /**< Table of supported device IDs */
	int (*probe)(struct pci_device *dev); /**< Probe function called when a device matches */
};

/**
 * @brief Read a 32-bit value from a PCI device's configuration space
 *
 * @param bus PCI bus number
 * @param slot PCI slot number
 * @param func PCI function number (0-7)
 * @param offset Offset within the PCI configuration space
 * @return uint32_t Value read from the register
 */
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/**
 * @brief Write a 32-bit value to a PCI device's configuration space
 *
 * @param bus PCI bus number
 * @param slot PCI slot number
 * @param func PCI function number (0-7)
 * @param offset Offset within the PCI configuration space
 * @param val Value to write
 */
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

/**
 * @brief Register a PCI driver in the system
 *
 * @param drv Pointer to the driver structure
 * @return int 0 on success, negative value on failure
 */
int pci_register_driver(struct pci_driver *drv);

/**
 * @brief Set bits in the PCI command register for a device
 *
 * @param dev Pointer to the PCI device
 * @param flags Bits to set (e.g., PCI_COMMAND_IO | PCI_COMMAND_MASTER)
 */
void pci_set_command(struct pci_device *dev, uint16_t flags);
