#include <efi.h>
#include <stdint.h>
#include <bootinfo/bootinfo.h>

#include <stddef.h>

#include <asm.h>
#include "higher_half.h"

#include <hubble/string.h>
#include "string.h"

EFI_HANDLE g_image;
EFI_SYSTEM_TABLE *g_systab;

// ACPI 2.0 (XSDT)
static EFI_GUID Acpi20Guid = {
    0x8868e871,
    0xe4f1,
    0x11d3,
    {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};

// ACPI 1.0 (RSDT)
static EFI_GUID Acpi10Guid = {
    0xeb9d2d30,
    0x2d88,
    0x11d3,
    {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};

static void *find_rsdp(EFI_SYSTEM_TABLE *SystemTable)
{
	for (UINTN i = 0; i < g_systab->NumberOfTableEntries; i++)
	{
		EFI_CONFIGURATION_TABLE *tbl = &g_systab->ConfigurationTable[i];

		if (memcmp(&tbl->VendorGuid, &Acpi20Guid, sizeof(EFI_GUID)) == 0)
		{
			// VERIFY THE SIGNATURE HERE
			char *sig = (char *)tbl->VendorTable;

			if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D')
				return tbl->VendorTable;
		}

		if (memcmp(&tbl->VendorGuid, &Acpi10Guid, sizeof(EFI_GUID)) == 0)
		{
			// VERIFY THE SIGNATURE HERE
			char *sig = (char *)tbl->VendorTable;

			if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D')
				return tbl->VendorTable;
		}
	}

	return NULL;
}

EFI_STATUS init_framebuffer(EFI_SYSTEM_TABLE *systab, framebuffer_info_t *fb_info)
{
	EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_STATUS status;

	status = systab->BootServices->LocateProtocol(&gop_guid, NULL, (void **)&gop);
	if (EFI_ERROR(status))
		return status;

	fb_info->base = (void *)gop->Mode->FrameBufferBase;
	fb_info->width = gop->Mode->Info->HorizontalResolution;
	fb_info->height = gop->Mode->Info->VerticalResolution;
	fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
	fb_info->bpp = 32;

	return EFI_SUCCESS;
}

EFI_STATUS open_file(EFI_SYSTEM_TABLE *systab, const CHAR16 *path, EFI_FILE_HANDLE *file)
{
	EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_HANDLE *handle_buffer = NULL;
	UINTN handle_count = 0;
	EFI_STATUS status;

	*file = NULL;

	status = systab->BootServices->LocateHandleBuffer(ByProtocol, &fs_guid, NULL, &handle_count, &handle_buffer);
	if (EFI_ERROR(status))
		return status;

	for (UINTN i = 0; i < handle_count; i++)
	{
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
		EFI_FILE_HANDLE root;

		status = systab->BootServices->HandleProtocol(handle_buffer[i], &fs_guid, (void **)&fs);
		if (EFI_ERROR(status))
			continue;

		status = fs->OpenVolume(fs, &root);
		if (EFI_ERROR(status))
			continue;

		status = root->Open(root, file, path, EFI_FILE_MODE_READ, 0);
		if (!EFI_ERROR(status))
		{
			root->Close(root);
			break;
		}

		root->Close(root);
	}

	if (handle_buffer)
		systab->BootServices->FreePool(handle_buffer);

	return (*file != NULL) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS get_file_size(EFI_FILE_HANDLE file, EFI_SYSTEM_TABLE *systab, UINTN *size)
{
	EFI_STATUS status;
	EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
	EFI_FILE_INFO *file_info = NULL;
	UINTN info_size = 0;

	// The first call is to get the size of the structure
	status = file->GetInfo(file, &FileInfoGuid, &info_size, NULL);
	if (status != EFI_BUFFER_TOO_SMALL)
		return status;

	// Allocating memory for the structure
	status = systab->BootServices->AllocatePool(EfiLoaderData, info_size, (void **)&file_info);
	if (EFI_ERROR(status))
		return status;

	// Getting the structure itself
	status = file->GetInfo(file, &FileInfoGuid, &info_size, file_info);
	if (!EFI_ERROR(status))
		*size = file_info->FileSize;

	systab->BootServices->FreePool(file_info);
	return status;
}

extern void jump_to_kernel(void *boot_info, void *entry, uint64_t stack);

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	g_image = image;
	g_systab = systab;

	EFI_STATUS status;

	// Open kernel file
	EFI_FILE_HANDLE KernelFile;
	status = open_file(systab, L"\\kernel.bin", &KernelFile);
	if (EFI_ERROR(status))
		return status;

	// Get kernel size
	UINTN kernel_size;
	status = get_file_size(KernelFile, systab, &kernel_size);
	if (EFI_ERROR(status))
		return status;

	// === [4] Allocate kernel at PHYSICAL address ===
	EFI_PHYSICAL_ADDRESS kernel_phys_addr = KERNEL_PHYS_BASE;
	UINTN kernel_pages = (kernel_size + 0xFFF) / 0x1000;

	status = systab->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, kernel_pages, &kernel_phys_addr);

	if (EFI_ERROR(status))
		return status;

	// === [5] Read kernel to PHYSICAL address ===
	status = KernelFile->Read(KernelFile, &kernel_size, (void *)kernel_phys_addr);
	if (EFI_ERROR(status))
		return status;

	KernelFile->Close(KernelFile);

	// === [6] Find largest memory region for heap ===
	EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
	UINTN mem_map_size = 0, map_key, desc_size;
	UINT32 desc_version;

	status = systab->BootServices->GetMemoryMap(&mem_map_size, mem_map, &map_key, &desc_size, &desc_version);

	if (status != EFI_BUFFER_TOO_SMALL)
		return status;

	mem_map_size += desc_size * 10;

	status = systab->BootServices->AllocatePool(EfiLoaderData, mem_map_size, (void **)&mem_map);

	if (EFI_ERROR(status))
		return status;

	status = systab->BootServices->GetMemoryMap(&mem_map_size, mem_map, &map_key, &desc_size, &desc_version);

	if (EFI_ERROR(status))
		return status;

	UINTN entry_count = mem_map_size / desc_size;

	EFI_MEMORY_DESCRIPTOR *largest = NULL;
	UINT64 largest_size = 0;

	for (UINTN i = 0; i < entry_count; i++)
	{
		EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mem_map + (i * desc_size));

		if (desc->Type == EfiConventionalMemory)
		{
			UINT64 size = desc->NumberOfPages * EFI_PAGE_SIZE;

			if (size > largest_size)
			{
				largest_size = size;
				largest = desc;
			}
		}
	}

	if (!largest || largest_size < 64 * 1024 * 1024)
		return EFI_OUT_OF_RESOURCES;

	// === [7] Reserve heap ===
	EFI_PHYSICAL_ADDRESS heap_phys = largest->PhysicalStart;
	UINT64 heap_size = largest->NumberOfPages * 0x1000;
	UINTN heap_pages = largest->NumberOfPages;

	status = systab->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, heap_pages, &heap_phys);

	if (EFI_ERROR(status))
		return status;

	// === [8] Create page tables with higher-half mapping ===

	// Allocate PML4
	EFI_PHYSICAL_ADDRESS pml4_addr = 0;
	status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pml4_addr);
	if (EFI_ERROR(status))
		return status;

	UINT64 *pml4 = (UINT64 *)pml4_addr;
	for (int i = 0; i < 512; i++)
		pml4[i] = 0;

	// === Create mappings ===

	// 1. Identity map first 4GB
	for (UINT64 phys = 0; phys < 0x100000000ULL; phys += 0x200000)
	{
		UINT64 pml4_idx = (phys >> 39) & 0x1FF;
		UINT64 pdpt_idx = (phys >> 30) & 0x1FF;
		UINT64 pd_idx = (phys >> 21) & 0x1FF;

		if (!(pml4[pml4_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pdpt_addr = 0;
			status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pdpt_addr);
			if (EFI_ERROR(status))
				return status;

			UINT64 *pdpt = (UINT64 *)pdpt_addr;
			for (int i = 0; i < 512; i++)
				pdpt[i] = 0;

			pml4[pml4_idx] = pdpt_addr | 0x3;
		}

		UINT64 *pdpt = (UINT64 *)(pml4[pml4_idx] & ~0xFFFULL);

		if (!(pdpt[pdpt_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pd_addr = 0;
			status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pd_addr);
			if (EFI_ERROR(status))
				return status;

			UINT64 *pd = (UINT64 *)pd_addr;
			for (int i = 0; i < 512; i++)
				pd[i] = 0;

			pdpt[pdpt_idx] = pd_addr | 0x3;
		}

		UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFULL);
		pd[pd_idx] = phys | 0x83; // Present + Write + Huge (2MB pages)
	}

	// 2. Higher-half mapping: 0xFFFFFFFF80000000 -> 0x0 (full 4GB)
	// ВАЖНО: Делаем это ДО recursive mapping, чтобы не перезаписать PML4[511]

	// Выделяем отдельный PDPT для higher-half (PML4[511])
	EFI_PHYSICAL_ADDRESS hh_pdpt_addr = 0;
	status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &hh_pdpt_addr);
	if (EFI_ERROR(status))
		return status;

	UINT64 *hh_pdpt = (UINT64 *)hh_pdpt_addr;
	for (int i = 0; i < 512; i++)
		hh_pdpt[i] = 0;

	// Устанавливаем PML4[511] для higher-half
	pml4[511] = hh_pdpt_addr | 0x3;

	// Теперь заполняем маппинг для 0xFFFFFFFF80000000 -> 0x0
	for (UINT64 offset = 0; offset < 0x100000000ULL; offset += 0x200000)
	{
		UINT64 virt = PHYS_TO_VIRT(offset);
		UINT64 phys = offset;
		UINT64 pml4_idx = (virt >> 39) & 0x1FF; // Будет 511
		UINT64 pdpt_idx = (virt >> 30) & 0x1FF; // 510 для 0x80000000
		UINT64 pd_idx = (virt >> 21) & 0x1FF;	// pml4[511] уже установлен выше
		UINT64 *pdpt = hh_pdpt;
		if (!(pdpt[pdpt_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pd_addr = 0;
			status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pd_addr);
			if (EFI_ERROR(status))
				return status;
			UINT64 *pd = (UINT64 *)pd_addr;
			for (int i = 0; i < 512; i++)
				pd[i] = 0;
			pdpt[pdpt_idx] = pd_addr | 0x3;
		}
		UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFULL);
		pd[pd_idx] = phys | 0x83;
	}

	for (UINT64 phys = 0xF0000000ULL; phys < 0x100000000ULL; phys += 0x200000)
	{
		UINT64 virt = PHYS_TO_VIRT(phys);
		UINT64 pml4_idx = (virt >> 39) & 0x1FF;
		UINT64 pdpt_idx = (virt >> 30) & 0x1FF;
		UINT64 pd_idx = (virt >> 21) & 0x1FF;
		UINT64 *pdpt = (UINT64 *)(pml4[pml4_idx] & ~0xFFFULL);
		UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFULL);
		pd[pd_idx] = phys | 0x83;
	}

	// 3. Recursive mapping в PML4[510] (не 511, так как там higher-half!)
	// Это позволит ядру манипулировать page tables
	pml4[510] = pml4_addr | 0x3;

	systab->BootServices->FreePool(mem_map);

	// === [9] Allocate and FILL boot info ===
	EFI_PHYSICAL_ADDRESS boot_info_addr = 0;

	status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &boot_info_addr);
	if (EFI_ERROR(status))
		return status;

	// === [10] allocate and fill rsdp ===
	// === [10] Find RSDP ===
	void *rsdp = find_rsdp(systab);
	if (!rsdp)
		return EFI_NOT_FOUND;

	char *sig = (char *)rsdp;

	// Check for "RSD PTR "
	if (sig[0] != 'R' || sig[1] != 'S' || sig[2] != 'D' || sig[3] != ' ' ||
	    sig[4] != 'P' || sig[5] != 'T' || sig[6] != 'R' || sig[7] != ' ')
		return EFI_INVALID_PARAMETER;

	// === [11] fill boot info ===

	BootInfo *boot_info = (BootInfo *)boot_info_addr;

	framebuffer_info_t *fb_info = &boot_info->framebuffer_data;
	status = init_framebuffer(systab, fb_info);
	if (EFI_ERROR(status))
		return status;

	ram_info_t *ram_info = &boot_info->memory_data;

	boot_info->rsdp = rsdp;

	ram_info->heap_start = heap_phys;
	ram_info->heap_size = heap_size;
	ram_info->pml4_phys = pml4_addr;

	boot_info->framebuffer = fb_info;
	boot_info->memory_map = ram_info;

	// === [9.5] Allocate stack BEFORE ExitBootServices ===
	EFI_PHYSICAL_ADDRESS stack_phys = 0;

	status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 16, &stack_phys);
	if (EFI_ERROR(status))
		return status;

	// Stack grows down, so point to top
	UINT64 stack_top = stack_phys + (16 * 0x1000);
	UINT64 stack_virt = PHYS_TO_VIRT(stack_top);

	// Вычисляем виртуальные адреса
	UINT64 kernel_virt_entry = PHYS_TO_VIRT(KERNEL_PHYS_BASE);
	UINT64 boot_info_virt = PHYS_TO_VIRT(boot_info_addr);

	// === [10] ExitBootServices ===
	mem_map = NULL;
	mem_map_size = 0;

	status = systab->BootServices->GetMemoryMap(&mem_map_size, mem_map, &map_key, &desc_size, &desc_version);

	if (status != EFI_BUFFER_TOO_SMALL)
		return status;

	mem_map_size += desc_size * 10;

	status = systab->BootServices->AllocatePool(EfiLoaderData, mem_map_size, (void **)&mem_map);
	if (EFI_ERROR(status))
		return status;

	status = systab->BootServices->GetMemoryMap(&mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
	if (EFI_ERROR(status))
		return status;

	status = systab->BootServices->ExitBootServices(image, map_key);
	if (EFI_ERROR(status))
	{
		systab->BootServices->FreePool(mem_map);
		return status;
	}

	// === POINT OF NO RETURN ===

	// === [11] Switch to new page tables and TEST ===
	set_cr3(pml4_addr);

	// === [12] Setup stack and jump ===
	jump_to_kernel((void *)boot_info_virt, (void *)kernel_virt_entry, stack_virt);

	// Should never reach here
	while (1)
		hlt();

	return EFI_SUCCESS;
}
