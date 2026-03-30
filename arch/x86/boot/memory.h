// memory.h
#pragma once
#include <efi.h>

typedef struct
{
	EFI_PHYSICAL_ADDRESS pml4_phys;
	EFI_PHYSICAL_ADDRESS heap_phys;
	UINT64 heap_size;
} boot_memory_t;

EFI_STATUS boot_memory_init(EFI_BOOT_SERVICES *bs, boot_memory_t *out);
