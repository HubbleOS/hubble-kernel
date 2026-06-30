/**
 * @file acpi.h
 * @brief ACPI table structures and public API
 *
 * Defines the standard ACPI table headers (RSDP, RSDT, XSDT,
 * FADT, MADT, HPET, MCFG) as packed C structs, callback type
 * definitions for hardware enumeration, and the public function
 * prototypes for ACPI initialisation and power management.
 */

#ifndef ACPI_H
#define ACPI_H

#include <stdbool.h>
#include <stdint.h>

/* ── ACPI SDT Header ─────────────────────────────────────────── */

/**
 * @brief Common header for all ACPI System Description Tables
 */
typedef struct
{
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OEMID[6];
	char OEMTableID[8];
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t CreatorRevision;
} __attribute__((packed)) ACPI_SDTHeader;

/* ── RSDP ────────────────────────────────────────────────────── */

/**
 * @brief Root System Description Pointer
 */
typedef struct
{
	char Signature[8];
	uint8_t Checksum;
	char OEMID[6];
	uint8_t Revision;
	uint32_t RsdtAddress;
	/* ACPI 2.0+ */
	uint32_t Length;
	uint64_t XsdtAddress;
	uint8_t ExtendedChecksum;
	uint8_t Reserved[3];
} __attribute__((packed)) RSDP;

/* ── RSDT / XSDT ─────────────────────────────────────────────── */

/**
 * @brief Root System Description Table (32-bit pointers)
 */
typedef struct
{
	ACPI_SDTHeader Header;
	uint32_t TablePointers[];
} __attribute__((packed)) RSDT;

/**
 * @brief Extended System Description Table (64-bit pointers)
 */
typedef struct
{
	ACPI_SDTHeader Header;
	uint64_t TablePointers[];
} __attribute__((packed)) XSDT;

/* ── Generic Address Structure ───────────────────────────────── */

/**
 * @brief ACPI Generic Address Structure
 */
typedef struct
{
	uint8_t AddressSpace;
	uint8_t BitWidth;
	uint8_t BitOffset;
	uint8_t AccessSize;
	uint64_t Address;
} __attribute__((packed)) GenericAddress;

/* ── FADT ─────────────────────────────────────────────────────── */

/**
 * @brief Fixed ACPI Description Table
 */
typedef struct
{
	ACPI_SDTHeader Header;
	uint32_t FirmwareCtrl;
	uint32_t Dsdt;
	uint8_t Reserved;
	uint8_t PreferredPowerManagementProfile;
	uint16_t SCI_Interrupt;
	uint32_t SMI_CommandPort;
	uint8_t AcpiEnable;
	uint8_t AcpiDisable;
	uint8_t S4BIOS_REQ;
	uint8_t PSTATE_Control;
	uint32_t PM1aEventBlock;
	uint32_t PM1bEventBlock;
	uint32_t PM1aControlBlock;
	uint32_t PM1bControlBlock;
	uint32_t PM2ControlBlock;
	uint32_t PMTimerBlock;
	uint32_t GPE0Block;
	uint32_t GPE1Block;
	uint8_t PM1EventLength;
	uint8_t PM1ControlLength;
	uint8_t PM2ControlLength;
	uint8_t PMTimerLength;
	uint8_t GPE0Length;
	uint8_t GPE1Length;
	uint8_t GPE1Base;
	uint8_t CStateControl;
	uint16_t WorstC2Latency;
	uint16_t WorstC3Latency;
	uint16_t FlushSize;
	uint16_t FlushStride;
	uint8_t DutyOffset;
	uint8_t DutyWidth;
	uint8_t DayAlarm;
	uint8_t MonthAlarm;
	uint8_t Century;
	uint16_t BootArchitectureFlags;
	uint8_t Reserved2;
	uint32_t Flags;
	GenericAddress ResetReg;
	uint8_t ResetValue;
	uint8_t Reserved3[3];
	uint64_t X_FirmwareControl;
	uint64_t X_Dsdt;
	GenericAddress X_PM1aEventBlock;
	GenericAddress X_PM1bEventBlock;
	GenericAddress X_PM1aControlBlock;
	GenericAddress X_PM1bControlBlock;
	GenericAddress X_PM2ControlBlock;
	GenericAddress X_PMTimerBlock;
	GenericAddress X_GPE0Block;
	GenericAddress X_GPE1Block;
} __attribute__((packed)) FADT;

/* ── MADT ────────────────────────────────────────────────────── */

/**
 * @brief Multiple APIC Description Table
 */
typedef struct
{
	ACPI_SDTHeader Header;
	uint32_t LocalAPICAddress;
	uint32_t Flags;
} __attribute__((packed)) MADT;

/**
 * @brief MADT entry header (Type / Length)
 */
typedef struct
{
	uint8_t Type;
	uint8_t Length;
} __attribute__((packed)) MADT_Entry;

/**
 * @brief Local APIC entry (Type 0)
 */
typedef struct
{
	MADT_Entry Header;
	uint8_t ProcessorID;
	uint8_t APIC_ID;
	uint32_t Flags;
} __attribute__((packed)) MADT_LAPIC;

/**
 * @brief I/O APIC entry (Type 1)
 */
typedef struct
{
	MADT_Entry Header;
	uint8_t IOAPIC_ID;
	uint8_t Reserved;
	uint32_t IOAPIC_Address;
	uint32_t GlobalSystemInterruptBase;
} __attribute__((packed)) MADT_IOAPIC;

/**
 * @brief Interrupt Source Override entry (Type 2)
 */
typedef struct
{
	MADT_Entry Header;
	uint8_t BusSource;
	uint8_t IRQSource;
	uint32_t GlobalSystemInterrupt;
	uint16_t Flags;
} __attribute__((packed)) MADT_ISO;

/* ── HPET ────────────────────────────────────────────────────── */

/**
 * @brief HPET ACPI table
 */
typedef struct
{
	ACPI_SDTHeader Header;
	uint32_t EventTimerBlockID;
	GenericAddress Address;
	uint8_t HPETNumber;
	uint16_t MinimumTick;
	uint8_t PageProtection;
} __attribute__((packed)) HPET;

/* ── MCFG ────────────────────────────────────────────────────── */

/**
 * @brief PCIe memory-mapped config space entry
 */
typedef struct
{
	uint64_t BaseAddress;
	uint16_t SegmentGroup;
	uint8_t StartBus;
	uint8_t EndBus;
	uint32_t Reserved;
} __attribute__((packed)) MCFG_Entry;

/**
 * @brief PCI Express MCFG table
 */
typedef struct
{
	ACPI_SDTHeader Header;
	uint64_t Reserved;
	MCFG_Entry Entries[];
} __attribute__((packed)) MCFG;

/* ── Callback Types ──────────────────────────────────────────── */

typedef void (*acpi_lapic_callback_t)(uint8_t apic_id, uint8_t processor_id, void *ctx);
typedef void (*acpi_ioapic_callback_t)(uint8_t ioapic_id, uint32_t address, uint32_t gsi_base, void *ctx);
typedef void (*acpi_iso_callback_t)(uint8_t irq_source, uint32_t gsi, uint16_t flags, void *ctx);
typedef void (*acpi_mcfg_callback_t)(uint64_t base_addr, uint16_t segment, uint8_t start_bus, uint8_t end_bus, void *ctx);

/* ── Core API ────────────────────────────────────────────────── */

int acpi_init(void *rsdp_ptr);
bool acpi_is_initialized(void);
void *acpi_find_sdt(const char *signature);
void acpi_shutdown(void);
void acpi_reboot(void);

/* ── Table Getters ───────────────────────────────────────────── */

FADT *acpi_get_fadt(void);
MADT *acpi_get_madt(void);
HPET *acpi_get_hpet(void);
MCFG *acpi_get_mcfg(void);

/* ── Hardware Enumeration ────────────────────────────────────── */

void acpi_enum_lapics(acpi_lapic_callback_t callback, void *ctx);
void acpi_enum_ioapics(acpi_ioapic_callback_t callback, void *ctx);
void acpi_enum_isos(acpi_iso_callback_t callback, void *ctx);
void acpi_enum_mcfg(acpi_mcfg_callback_t callback, void *ctx);

/* ── Convenience Functions ───────────────────────────────────── */

uint64_t acpi_get_hpet_address(void);
uint64_t acpi_get_lapic_address(void);

#endif /* ACPI_H */
