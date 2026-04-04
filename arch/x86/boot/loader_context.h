#pragma once
#include <stdint.h>

// Один дескриптор пам'яті (EFI-незалежний)
typedef enum
{
	MEM_CONVENTIONAL = 0, // вільна RAM
	MEM_RESERVED,
	MEM_LOADER_CODE,
	MEM_LOADER_DATA,
	MEM_ACPI,
	MEM_MMIO,
	MEM_OTHER,
} mem_type_t;

typedef struct
{
	uint64_t phys_start;
	uint64_t num_pages;
	mem_type_t type;
} mem_descriptor_t;

typedef struct
{
	void *base;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t bpp;
} fb_info_t;

// Всі ресурси, які efi_main передає boot_main
typedef struct
{
	void *elf_buf;
	// UINTN elf_size;
	unsigned long long elf_size;

	// Пам'ять
	mem_descriptor_t *mem_map; // масив дескрипторів
	uint64_t mem_map_count;

	// Фреймбуфер
	fb_info_t framebuffer;

	// ACPI
	void *rsdp;

	// Ядро (вже завантажене)
	uint64_t kernel_phys; // фізична адреса ELF/flat binary
	uint64_t kernel_size;

	// Де виділяти сторінки для page tables / стека / boot_info
	// (boot_main сам будує page tables)
	uint64_t free_phys_base; // найбільший вільний регіон
	uint64_t free_phys_size;
} loader_context_t;
