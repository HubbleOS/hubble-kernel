/**
 * @file rsdp.h
 * @brief Root System Description Pointer (RSDP) structure
 *
 * Defines the RSDP structure used by ACPI to locate the
 * Root System Description Table (RSDT) or Extended SDT (XSDT).
 */

#pragma once

#include <stdint.h>

/**
 * @brief ACPI Root System Description Pointer
 *
 * This structure is located by the bootloader in the EBDA or
 * BIOS memory area and points to the RSDT/XSDT.
 */
typedef struct
{
	char Signature[8];
	uint8_t Checksum;
	char OEMID[6];
	uint8_t Revision;
	uint32_t RsdtAddress;

	/* ACPI 2.0 extended section */
	uint32_t Length;
	uint64_t XsdtAddress;
	uint8_t ExtendedChecksum;
	uint8_t reserved[3];
} __attribute__((packed)) rsdp_t;
