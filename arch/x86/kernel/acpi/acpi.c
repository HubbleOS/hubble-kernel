/**
 * @file acpi.c
 * @brief ACPI table parsing and power management
 *
 * Locates and caches the FADT, MADT, HPET, and MCFG tables
 * from the RSDT/XSDT, provides lookup helpers for hardware
 * enumeration (LAPICs, I/O APICs, ISOs, PCIe segments),
 * and implements ACPI-based shutdown and reboot.
 */

#include "acpi.h"
#include <asm.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>

#include "higher_half.h"

/* -- Global ACPI State ----------------------------------------- */

static struct {
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

/* -- SDT Iterator ---------------------------------------------- */

typedef void (*sdt_callback_t)(ACPI_SDTHeader *table, void *ctx);

/**
 * @brief Iterate all entries in the RSDT or XSDT
 *
 * @param callback Function called for each SDT
 * @param ctx      Opaque context pointer
 */
static void iterate_sdt_entries(sdt_callback_t callback, void *ctx) {
  if (acpi_state.xsdt) {
    uint32_t entries =
        (acpi_state.xsdt->Header.Length - sizeof(ACPI_SDTHeader)) /
        sizeof(uint64_t);
    for (uint32_t i = 0; i < entries; i++) {
      ACPI_SDTHeader *tbl =
          (ACPI_SDTHeader *)phys_to_virt(acpi_state.xsdt->TablePointers[i]);
      callback(tbl, ctx);
    }
  } else if (acpi_state.rsdt) {
    uint32_t entries =
        (acpi_state.rsdt->Header.Length - sizeof(ACPI_SDTHeader)) /
        sizeof(uint32_t);
    for (uint32_t i = 0; i < entries; i++) {
      ACPI_SDTHeader *tbl =
          (ACPI_SDTHeader *)phys_to_virt(acpi_state.rsdt->TablePointers[i]);
      callback(tbl, ctx);
    }
  }
}

/* -- DSDT / _S5 Parsing (static) ------------------------------- */

/**
 * @brief Parse _S5 from DSDT AML bytecode
 *
 * Locates the \_S5 object and extracts SLP_TYPa / SLP_TYPb
 * values used for ACPI shutdown.
 *
 * @param aml  Pointer to DSDT definition block
 * @param len  Length of the AML blob
 * @return true if _S5 was found and parsed
 */
static bool parse_s5(uint8_t *aml, uint32_t len) {
  for (uint32_t i = 0; i < len - 4; i++) {
    if (memcmp(&aml[i], "_S5_", 4) != 0)
      continue;

    uint32_t off = i + 4;
    while (off < len && aml[off] != 0x12)
      off++;

    if (off >= len - 3)
      return false;

    off += 3;

    if (aml[off] == 0x0A) {
      acpi_state.slp_typa = aml[off + 1];
      off += 2;
    }
    if (off < len && aml[off] == 0x0A) {
      acpi_state.slp_typb = aml[off + 1];
    }

    printk(KERN_INFO "_S5 found: SLP_TYPa=0x%x, SLP_TYPb=0x%x\n",
           acpi_state.slp_typa, acpi_state.slp_typb);
    return true;
  }
  return false;
}

/* -- FADT Processing (static) ---------------------------------- */

/**
 * @brief Process the FADT table, cache it, and parse DSDT for _S5
 */
static void process_fadt(ACPI_SDTHeader *tbl, void *ctx) {
  (void)ctx;
  if (memcmp(tbl->Signature, "FACP", 4) != 0)
    return;

  acpi_state.fadt = (FADT *)tbl;
  printk(KERN_INFO "FADT found at %p\n", acpi_state.fadt);

  uint64_t dsdt_phys =
      acpi_state.fadt->X_Dsdt ? acpi_state.fadt->X_Dsdt : acpi_state.fadt->Dsdt;
  if (!dsdt_phys) {
    printk(KERN_ERR "DSDT pointer missing\n");
    return;
  }

  ACPI_SDTHeader *dsdt = (ACPI_SDTHeader *)phys_to_virt(dsdt_phys);
  if (memcmp(dsdt->Signature, "DSDT", 4) != 0) {
    printk(KERN_ERR "Invalid DSDT signature\n");
    return;
  }

  printk(KERN_INFO "DSDT found at %p (length=%u)\n", dsdt, dsdt->Length);

  uint8_t *aml = (uint8_t *)dsdt + sizeof(ACPI_SDTHeader);
  uint32_t aml_len = dsdt->Length - sizeof(ACPI_SDTHeader);

  if (parse_s5(aml, aml_len)) {
    acpi_state.shutdown_ready = true;
    printk(KERN_OK "ACPI shutdown ready\n");
  } else {
    printk(KERN_ERR "_S5 not found in DSDT\n");
  }
}

/* -- Table Caching (static) ------------------------------------ */

/**
 * @brief Cache known table pointers (MADT, HPET, MCFG)
 */
static void cache_tables(ACPI_SDTHeader *tbl, void *ctx) {
  (void)ctx;
  if (memcmp(tbl->Signature, "APIC", 4) == 0) {
    acpi_state.madt = (MADT *)tbl;
    printk(KERN_INFO "MADT found at %p\n", tbl);
  } else if (memcmp(tbl->Signature, "HPET", 4) == 0) {
    acpi_state.hpet = (HPET *)tbl;
    printk(KERN_INFO "HPET found at %p\n", tbl);
  } else if (memcmp(tbl->Signature, "MCFG", 4) == 0) {
    acpi_state.mcfg = (MCFG *)tbl;
    printk(KERN_INFO "MCFG found at %p\n", tbl);
  }
}

/* -- SDT Lookup Helper (static) -------------------------------- */

/**
 * @brief Callback wrapper for acpi_find_sdt
 */
static void find_sdt_callback(ACPI_SDTHeader *tbl, void *ctx) {
  struct {
    const char *sig;
    ACPI_SDTHeader **result;
  } *params = ctx;

  if (memcmp(tbl->Signature, params->sig, 4) == 0)
    *params->result = tbl;
}

/* -- Public API ------------------------------------------------ */

/**
 * @brief Find an arbitrary SDT by 4-character signature
 *
 * @param signature 4-character table signature
 * @return Pointer to the table, or NULL if not found
 */
void *acpi_find_sdt(const char *signature) {
  ACPI_SDTHeader *result = NULL;
  struct {
    const char *sig;
    ACPI_SDTHeader **result;
  } params = {signature, &result};

  iterate_sdt_entries(find_sdt_callback, &params);
  return result;
}

/**
 * @brief Initialise the ACPI subsystem from the bootloader-provided RSDP
 *
 * @param rsdp_ptr Physical address of the RSDP
 * @return 0 on success, -1 on failure
 */
int acpi_init(void *rsdp_ptr) {
  if (!rsdp_ptr) {
    printk(KERN_INFO "No RSDP provided\n");
    return -1;
  }

  RSDP *rsdp = (RSDP *)phys_to_virt((uint64_t)rsdp_ptr);

  if (memcmp(rsdp->Signature, "RSD PTR ", 8) != 0) {
    printk(KERN_ERR "Invalid RSDP signature\n");
    return -1;
  }

  printk(KERN_INFO "RSDP found (rev %d) at phys=%p\n", rsdp->Revision,
         rsdp_ptr);

  if (rsdp->Revision >= 2 && rsdp->XsdtAddress) {
    acpi_state.xsdt = (XSDT *)phys_to_virt(rsdp->XsdtAddress);
    if (memcmp(acpi_state.xsdt->Header.Signature, "XSDT", 4) != 0) {
      printk(KERN_ERR "Invalid XSDT\n");
      return -1;
    }
    printk(KERN_INFO "Using XSDT at %p\n", acpi_state.xsdt);
  } else {
    acpi_state.rsdt = (RSDT *)phys_to_virt(rsdp->RsdtAddress);
    if (memcmp(acpi_state.rsdt->Header.Signature, "RSDT", 4) != 0) {
      printk(KERN_ERR "Invalid RSDT\n");
      return -1;
    }
    printk(KERN_INFO "Using RSDT at %p\n", acpi_state.rsdt);
  }

  iterate_sdt_entries(process_fadt, NULL);
  iterate_sdt_entries(cache_tables, NULL);

  acpi_state.initialized = true;
  printk(KERN_OK "ACPI initialized\n");
  return 0;
}

/* -- Power Management ------------------------------------------ */

/**
 * @brief Shut down the machine via ACPI _S5 or fallback ports
 */
void acpi_shutdown(void) {
  if (!acpi_state.shutdown_ready || !acpi_state.fadt) {
    printk(KERN_ERR "ACPI shutdown unavailable, trying fallback methods\n");

    cli();
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    printk(KERN_ERR "Shutdown failed, halting\n");
    while (1)
      hlt();
  }

  printk(KERN_INFO "Shutting down via ACPI...\n");
  cli();

  uint16_t pm1a = acpi_state.fadt->PM1aControlBlock;
  uint16_t pm1b = acpi_state.fadt->PM1bControlBlock;

  if (pm1a)
    outw(pm1a, (acpi_state.slp_typa << 10) | (1 << 13));
  if (pm1b)
    outw(pm1b, (acpi_state.slp_typb << 10) | (1 << 13));

  printk(KERN_ERR "ACPI shutdown failed, halting\n");
  while (1)
    hlt();
}

/**
 * @brief Reboot the machine via ACPI reset register or fallback
 */
void acpi_reboot(void) {
  if (!acpi_state.fadt) {
    printk(KERN_ERR "ACPI reboot unavailable, trying fallback\n");

    cli();
    outb(0x64, 0xFE);

    asm volatile("lidt 0; int3");
    while (1)
      hlt();
  }

  printk(KERN_INFO "Rebooting via ACPI...\n");
  cli();

  if (acpi_state.fadt->ResetReg.Address) {
    uint8_t reset_value = acpi_state.fadt->ResetValue;
    uint64_t addr = acpi_state.fadt->ResetReg.Address;

    switch (acpi_state.fadt->ResetReg.AddressSpace) {
    case 0:
      *(volatile uint8_t *)phys_to_virt(addr) = reset_value;
      break;
    case 1:
      outb((uint16_t)addr, reset_value);
      break;
    }
  }

  outb(0x64, 0xFE);
  asm volatile("lidt 0; int3");
  while (1)
    hlt();
}

/* -- Status / Getters ------------------------------------------ */

bool acpi_is_initialized(void) {
  printk(KERN_OK "ACPI initialized: %d\n", acpi_state.initialized);
  return acpi_state.initialized;
}

FADT *acpi_get_fadt(void) { return acpi_state.fadt; }
MADT *acpi_get_madt(void) { return acpi_state.madt; }
HPET *acpi_get_hpet(void) { return acpi_state.hpet; }
MCFG *acpi_get_mcfg(void) { return acpi_state.mcfg; }

/* -- MADT Enumeration ------------------------------------------ */

void acpi_enum_lapics(acpi_lapic_callback_t callback, void *ctx) {
  if (!acpi_state.madt) {
    printk(KERN_INFO "MADT not available\n");
    return;
  }

  uint8_t *ptr = (uint8_t *)acpi_state.madt + sizeof(MADT);
  uint8_t *end = (uint8_t *)acpi_state.madt + acpi_state.madt->Header.Length;

  while (ptr < end) {
    MADT_Entry *entry = (MADT_Entry *)ptr;

    if (entry->Type == 0) {
      MADT_LAPIC *lapic = (MADT_LAPIC *)entry;
      if (lapic->Flags & 1)
        callback(lapic->APIC_ID, lapic->ProcessorID, ctx);
    }

    ptr += entry->Length;
  }
}

void acpi_enum_ioapics(acpi_ioapic_callback_t callback, void *ctx) {
  if (!acpi_state.madt) {
    printk(KERN_INFO "MADT not available\n");
    return;
  }

  uint8_t *ptr = (uint8_t *)acpi_state.madt + sizeof(MADT);
  uint8_t *end = (uint8_t *)acpi_state.madt + acpi_state.madt->Header.Length;

  while (ptr < end) {
    MADT_Entry *entry = (MADT_Entry *)ptr;

    if (entry->Type == 1) {
      MADT_IOAPIC *ioapic = (MADT_IOAPIC *)entry;
      callback(ioapic->IOAPIC_ID, ioapic->IOAPIC_Address,
               ioapic->GlobalSystemInterruptBase, ctx);
    }

    ptr += entry->Length;
  }
}

void acpi_enum_isos(acpi_iso_callback_t callback, void *ctx) {
  if (!acpi_state.madt) {
    printk(KERN_INFO "MADT not available\n");
    return;
  }

  uint8_t *ptr = (uint8_t *)acpi_state.madt + sizeof(MADT);
  uint8_t *end = (uint8_t *)acpi_state.madt + acpi_state.madt->Header.Length;

  while (ptr < end) {
    MADT_Entry *entry = (MADT_Entry *)ptr;

    if (entry->Type == 2) {
      MADT_ISO *iso = (MADT_ISO *)entry;
      callback(iso->IRQSource, iso->GlobalSystemInterrupt, iso->Flags, ctx);
    }

    ptr += entry->Length;
  }
}

void acpi_enum_mcfg(acpi_mcfg_callback_t callback, void *ctx) {
  if (!acpi_state.mcfg) {
    printk(KERN_INFO "MCFG not available\n");
    return;
  }

  uint32_t entries =
      (acpi_state.mcfg->Header.Length - sizeof(MCFG)) / sizeof(MCFG_Entry);

  for (uint32_t i = 0; i < entries; i++) {
    MCFG_Entry *entry = &acpi_state.mcfg->Entries[i];
    callback(entry->BaseAddress, entry->SegmentGroup, entry->StartBus,
             entry->EndBus, ctx);
  }
}

/* -- Convenience ----------------------------------------------- */

uint64_t acpi_get_hpet_address(void) {
  if (!acpi_state.hpet)
    return 0;

  return acpi_state.hpet->Address.Address;
}

uint64_t acpi_get_lapic_address(void) {
  if (!acpi_state.madt)
    return 0xFEE00000;

  return acpi_state.madt->LocalAPICAddress;
}
