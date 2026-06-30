/**
 * @file loader_context.h
 * @brief Bootloader context structure — resources passed from EFI to the kernel entry
 */

#pragma once

#include <stdint.h>

/** @brief Memory region type (EFI-independent) */
typedef enum
{
	MEM_CONVENTIONAL = 0,
	MEM_RESERVED,
	MEM_LOADER_CODE,
	MEM_LOADER_DATA,
	MEM_ACPI,
	MEM_MMIO,
	MEM_OTHER,
} mem_type_t;

/** @brief Single memory region descriptor */
typedef struct
{
	uint64_t phys_start;
	uint64_t num_pages;
	mem_type_t type;
} mem_descriptor_t;

/** @brief Framebuffer information */
typedef struct
{
	void *base;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t bpp;
} fb_info_t;

/** @brief All resources passed from efi_main to boot_main */
typedef struct
{
	void *elf_buf;
	unsigned long long elf_size;

	mem_descriptor_t *mem_map;
	uint64_t mem_map_count;

	fb_info_t framebuffer;

	void *rsdp;

	uint64_t kernel_phys;
	uint64_t kernel_size;

	uint64_t free_phys_base;
	uint64_t free_phys_size;
} loader_context_t;
