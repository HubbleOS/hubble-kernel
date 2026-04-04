#include "acpi.h"
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>
#include "higher_half.h"

#include <asm.h>

// Global ACPI state
static struct
{
	XSDT *xsdt;
	RSDT *rsdt;
	FADT *fadt;
	MADT *madt;
	HPET *hpet;
	MCFG *mcfg;
	uint16_t slp_typa;
	uint16_t slp_typb;
	bool shutdown_ready;
	bool initialized;
} acpi_state = {0};

// Helper to iterate through SDT entries
typedef void (*sdt_callback_t)(ACPI_SDTHeader *table, void *ctx);

static void iterate_sdt_entries(sdt_callback_t callback, void *ctx)
{
	if (acpi_state.xsdt)
	{
		uint32_t entries = (acpi_state.xsdt->Header.Length - sizeof(ACPI_SDTHeader)) / sizeof(uint64_t);
		for (uint32_t i = 0; i < entries; i++)
		{
			ACPI_SDTHeader *tbl = (ACPI_SDTHeader *)(DIRECT_MAP_BASE + acpi_state.xsdt->TablePointers[i]);

			callback(tbl, ctx);
		}
	}
	else if (acpi_state.rsdt)
	{
		uint32_t entries = (acpi_state.rsdt->Header.Length - sizeof(ACPI_SDTHeader)) / sizeof(uint32_t);
		for (uint32_t i = 0; i < entries; i++)
		{
			ACPI_SDTHeader *tbl = (ACPI_SDTHeader *)(DIRECT_MAP_BASE + (uint64_t)acpi_state.rsdt->TablePointers[i]);

			callback(tbl, ctx);
		}
	}
}

// Parse _S5 from DSDT to get shutdown values
static bool parse_s5(uint8_t *aml, uint32_t len)
{
	for (uint32_t i = 0; i < len - 4; i++)
	{
		if (memcmp(&aml[i], "_S5_", 4) != 0)
			continue;

		// Found _S5_, look for package (0x12)
		uint32_t off = i + 4;
		while (off < len && aml[off] != 0x12)
			off++;

		if (off >= len - 3)
			return false;

		off += 3; // Skip: package_op, pkg_length, num_elements

		// Parse SLP_TYPa and SLP_TYPb
		if (aml[off] == 0x0A)
		{
			acpi_state.slp_typa = aml[off + 1];
			off += 2;
		}
		if (off < len && aml[off] == 0x0A)
		{
			acpi_state.slp_typb = aml[off + 1];
		}

		printk("_S5 found: SLP_TYPa=0x%x, SLP_TYPb=0x%x\n", acpi_state.slp_typa, acpi_state.slp_typb);
		return true;
	}
	return false;
}

// Process FADT table
static void process_fadt(ACPI_SDTHeader *tbl, void *ctx)
{
	if (memcmp(tbl->Signature, "FACP", 4) != 0)
		return;

	acpi_state.fadt = (FADT *)tbl;
	printk("FADT found at %p\n", acpi_state.fadt);

	// Get DSDT pointer (prefer X_Dsdt for 64-bit)
	uint64_t dsdt_phys = acpi_state.fadt->X_Dsdt ? acpi_state.fadt->X_Dsdt : acpi_state.fadt->Dsdt;
	if (!dsdt_phys)
	{
		printk("DSDT pointer missing\n");
		return;
	}

	ACPI_SDTHeader *dsdt = (ACPI_SDTHeader *)(DIRECT_MAP_BASE + dsdt_phys);

	if (memcmp(dsdt->Signature, "DSDT", 4) != 0)
	{
		printk("Invalid DSDT signature\n");
		return;
	}

	printk("DSDT found at %p (length=%u)\n", dsdt, dsdt->Length);

	// Parse DSDT for _S5
	uint8_t *aml = (uint8_t *)dsdt + sizeof(ACPI_SDTHeader);
	uint32_t aml_len = dsdt->Length - sizeof(ACPI_SDTHeader);

	if (parse_s5(aml, aml_len))
	{
		acpi_state.shutdown_ready = true;
		printk("ACPI shutdown ready\n");
	}
	else
	{
		printk("_S5 not found in DSDT\n");
	}
}

// Cache common tables
static void cache_tables(ACPI_SDTHeader *tbl, void *ctx)
{
	if (memcmp(tbl->Signature, "APIC", 4) == 0)
	{
		acpi_state.madt = (MADT *)tbl;
		printk("MADT found at %p\n", tbl);
	}
	else if (memcmp(tbl->Signature, "HPET", 4) == 0)
	{
		acpi_state.hpet = (HPET *)tbl;
		printk("HPET found at %p\n", tbl);
	}
	else if (memcmp(tbl->Signature, "MCFG", 4) == 0)
	{
		acpi_state.mcfg = (MCFG *)tbl;
		printk("MCFG found at %p\n", tbl);
	}
}

// Find specific SDT by signature
static void find_sdt_callback(ACPI_SDTHeader *tbl, void *ctx)
{
	struct
	{
		const char *sig;
		ACPI_SDTHeader **result;
	} *params = ctx;

	if (memcmp(tbl->Signature, params->sig, 4) == 0)
		*params->result = tbl;
}

void *acpi_find_sdt(const char *signature)
{
	ACPI_SDTHeader *result = NULL;
	struct
	{
		const char *sig;
		ACPI_SDTHeader **result;
	} params = {signature, &result};

	iterate_sdt_entries(find_sdt_callback, &params);
	return result;
}

int acpi_init(void *rsdp_ptr)
{
	if (!rsdp_ptr)
	{
		printk("No RSDP provided\n");
		return -1;
	}

	RSDP *rsdp = (RSDP *)(DIRECT_MAP_BASE + (uintptr_t)rsdp_ptr);

	// Verify RSDP signature
	if (memcmp(rsdp->Signature, "RSD PTR ", 8) != 0)
	{
		printk("Invalid RSDP signature\n");
		return -1;
	}

	printk("RSDP found (rev %d) at phys=%p\n", rsdp->Revision, rsdp_ptr);

	// Get XSDT or RSDT
	if (rsdp->Revision >= 2 && rsdp->XsdtAddress)
	{
		acpi_state.xsdt = (XSDT *)(DIRECT_MAP_BASE + rsdp->XsdtAddress);

		if (memcmp(acpi_state.xsdt->Header.Signature, "XSDT", 4) != 0)
		{
			printk("Invalid XSDT\n");
			return -1;
		}
		printk("Using XSDT at %p\n", acpi_state.xsdt);
	}
	else
	{
		acpi_state.rsdt = (RSDT *)(DIRECT_MAP_BASE + (uint64_t)rsdp->RsdtAddress);

		if (memcmp(acpi_state.rsdt->Header.Signature, "RSDT", 4) != 0)
		{
			printk("Invalid RSDT\n");
			return -1;
		}
		printk("Using RSDT at %p\n", acpi_state.rsdt);
	}

	// Find and cache important tables
	iterate_sdt_entries(process_fadt, NULL);
	iterate_sdt_entries(cache_tables, NULL);

	acpi_state.initialized = true;
	printk("ACPI initialized\n");
	return 0;
}

void acpi_shutdown(void)
{
	if (!acpi_state.shutdown_ready || !acpi_state.fadt)
	{
		printk("ACPI shutdown unavailable, trying fallback methods\n");

		// Try common QEMU/Bochs shutdown ports
		cli();
		outw(0x604, 0x2000);  // QEMU
		outw(0xB004, 0x2000); // Old QEMU
		outw(0x4004, 0x3400); // Bochs

		printk("Shutdown failed, halting\n");
		while (1)
			hlt();
	}

	printk("Shutting down via ACPI...\n");
	cli();

	// Write SLP_TYP | SLP_EN to PM1 control blocks
	uint16_t pm1a = acpi_state.fadt->PM1aControlBlock;
	uint16_t pm1b = acpi_state.fadt->PM1bControlBlock;

	if (pm1a)
		outw(pm1a, (acpi_state.slp_typa << 10) | (1 << 13));
	if (pm1b)
		outw(pm1b, (acpi_state.slp_typb << 10) | (1 << 13));

	// Fallback if ACPI method didn't work
	printk("ACPI shutdown failed, halting\n");
	while (1)
		hlt();
}

void acpi_reboot(void)
{
	if (!acpi_state.fadt)
	{
		printk("ACPI reboot unavailable, trying fallback\n");

		// Try keyboard controller reset
		cli();
		outb(0x64, 0xFE);

		// Triple fault as last resort
		asm volatile("lidt 0; int3");
		while (1)
			hlt();
	}

	printk("Rebooting via ACPI...\n");
	cli();

	// Use FADT reset register if available
	if (acpi_state.fadt->ResetReg.Address)
	{
		uint8_t reset_value = acpi_state.fadt->ResetValue;
		uint64_t addr = acpi_state.fadt->ResetReg.Address;

		switch (acpi_state.fadt->ResetReg.AddressSpace)
		{
		case 0: // System Memory
			*(volatile uint8_t *)(DIRECT_MAP_BASE + addr) = reset_value;
			break;
		case 1: // System I/O
			outb((uint16_t)addr, reset_value);
			break;
		}
	}

	// Fallback methods
	outb(0x64, 0xFE);	      // Keyboard controller
	asm volatile("lidt 0; int3"); // Triple fault
	while (1)
		hlt();
}

// === Public API Functions ===

bool acpi_is_initialized(void)
{
	printk("ACPI initialized: %d\n", acpi_state.initialized);
	return acpi_state.initialized;
}

FADT *acpi_get_fadt(void)
{
	return acpi_state.fadt;
}

MADT *acpi_get_madt(void)
{
	return acpi_state.madt;
}

HPET *acpi_get_hpet(void)
{
	return acpi_state.hpet;
}

MCFG *acpi_get_mcfg(void)
{
	return acpi_state.mcfg;
}

// Enumerate Local APICs from MADT
void acpi_enum_lapics(acpi_lapic_callback_t callback, void *ctx)
{
	if (!acpi_state.madt)
	{
		printk("MADT not available\n");
		return;
	}

	uint8_t *ptr = (uint8_t *)acpi_state.madt + sizeof(MADT);
	uint8_t *end = (uint8_t *)acpi_state.madt + acpi_state.madt->Header.Length;

	while (ptr < end)
	{
		MADT_Entry *entry = (MADT_Entry *)ptr;

		if (entry->Type == 0) // Local APIC
		{
			MADT_LAPIC *lapic = (MADT_LAPIC *)entry;
			if (lapic->Flags & 1) // Enabled
			{
				callback(lapic->APIC_ID, lapic->ProcessorID, ctx);
			}
		}

		ptr += entry->Length;
	}
}

// Enumerate I/O APICs from MADT
void acpi_enum_ioapics(acpi_ioapic_callback_t callback, void *ctx)
{
	if (!acpi_state.madt)
	{
		printk("MADT not available\n");
		return;
	}

	uint8_t *ptr = (uint8_t *)acpi_state.madt + sizeof(MADT);
	uint8_t *end = (uint8_t *)acpi_state.madt + acpi_state.madt->Header.Length;

	while (ptr < end)
	{
		MADT_Entry *entry = (MADT_Entry *)ptr;

		if (entry->Type == 1) // I/O APIC
		{
			MADT_IOAPIC *ioapic = (MADT_IOAPIC *)entry;
			callback(ioapic->IOAPIC_ID, ioapic->IOAPIC_Address,
				 ioapic->GlobalSystemInterruptBase, ctx);
		}

		ptr += entry->Length;
	}
}

// Enumerate ISO entries (Interrupt Source Override) from MADT
void acpi_enum_isos(acpi_iso_callback_t callback, void *ctx)
{
	if (!acpi_state.madt)
	{
		printk("MADT not available\n");
		return;
	}

	uint8_t *ptr = (uint8_t *)acpi_state.madt + sizeof(MADT);
	uint8_t *end = (uint8_t *)acpi_state.madt + acpi_state.madt->Header.Length;

	while (ptr < end)
	{
		MADT_Entry *entry = (MADT_Entry *)ptr;

		if (entry->Type == 2) // Interrupt Source Override
		{
			MADT_ISO *iso = (MADT_ISO *)entry;
			callback(iso->IRQSource, iso->GlobalSystemInterrupt,
				 iso->Flags, ctx);
		}

		ptr += entry->Length;
	}
}

// Get HPET base address
uint64_t acpi_get_hpet_address(void)
{
	if (!acpi_state.hpet)
		return 0;

	return acpi_state.hpet->Address.Address;
}

// Get Local APIC address from MADT
uint64_t acpi_get_lapic_address(void)
{
	if (!acpi_state.madt)
		return 0xFEE00000; // Default LAPIC address

	return acpi_state.madt->LocalAPICAddress;
}

// Enumerate PCI Express memory-mapped config spaces
void acpi_enum_mcfg(acpi_mcfg_callback_t callback, void *ctx)
{
	if (!acpi_state.mcfg)
	{
		printk("MCFG not available\n");
		return;
	}

	uint32_t entries = (acpi_state.mcfg->Header.Length - sizeof(MCFG)) / sizeof(MCFG_Entry);

	for (uint32_t i = 0; i < entries; i++)
	{
		MCFG_Entry *entry = &acpi_state.mcfg->Entries[i];
		callback(entry->BaseAddress, entry->SegmentGroup,
			 entry->StartBus, entry->EndBus, ctx);
	}
}
