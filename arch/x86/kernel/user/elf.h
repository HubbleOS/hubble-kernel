/**
 * @file elf.h
 * @brief ELF file format definitions
 *
 * This file contains definitions for the ELF (Executable and Linkable Format)
 * file format used in the x86 architecture.
 */

#pragma once

#include <stdint.h>

typedef struct
{
	unsigned char e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

int elf_load(const char *path, uint64_t *entry_out);
