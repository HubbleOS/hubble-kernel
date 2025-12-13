#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <bootinfo/bootinfo.h>
#include "globals.h"
#include "console.h"

#include "asm.h"
#include "higher_half.h"

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
	Print(L"=== Searching for RSDP ===\n");
	Print(L"Configuration table entries: %u\n", SystemTable->NumberOfTableEntries);

	for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++)
	{
		EFI_CONFIGURATION_TABLE *tbl = &SystemTable->ConfigurationTable[i];

		// Print GUID for debugging
		Print(L"Entry %u: GUID = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		      i,
		      tbl->VendorGuid.Data1,
		      tbl->VendorGuid.Data2,
		      tbl->VendorGuid.Data3,
		      tbl->VendorGuid.Data4[0],
		      tbl->VendorGuid.Data4[1],
		      tbl->VendorGuid.Data4[2],
		      tbl->VendorGuid.Data4[3],
		      tbl->VendorGuid.Data4[4],
		      tbl->VendorGuid.Data4[5],
		      tbl->VendorGuid.Data4[6],
		      tbl->VendorGuid.Data4[7]);

		if (CompareGuid(&tbl->VendorGuid, &Acpi20Guid))
		{
			Print(L"Found ACPI 2.0 RSDP at %p\n", tbl->VendorTable);

			// VERIFY THE SIGNATURE HERE
			char *sig = (char *)tbl->VendorTable;
			Print(L"Signature bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			      sig[0], sig[1], sig[2], sig[3], sig[4], sig[5], sig[6], sig[7]);
			Print(L"As string: %.8s\n", sig);

			if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D')
			{
				Print(L"Signature VALID!\n");
				return tbl->VendorTable;
			}
			else
			{
				Print(L"Signature INVALID! Continuing search...\n");
			}
		}

		if (CompareGuid(&tbl->VendorGuid, &Acpi10Guid))
		{
			Print(L"Found ACPI 1.0 RSDP at %p\n", tbl->VendorTable);

			// VERIFY THE SIGNATURE HERE
			char *sig = (char *)tbl->VendorTable;
			Print(L"Signature bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			      sig[0], sig[1], sig[2], sig[3], sig[4], sig[5], sig[6], sig[7]);
			Print(L"As string: %.8s\n", sig);

			if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D')
			{
				Print(L"Signature VALID!\n");
				return tbl->VendorTable;
			}
			else
			{
				Print(L"Signature INVALID! Continuing search...\n");
			}
		}
	}

	Print(L"ACPI RSDP not found in any configuration table\n");
	return NULL;
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	g_image = image;
	g_systab = systab;

	InitializeLib(image, systab);
	ClearConsole();

	PrintMessage(L"BOOT", EFI_LIGHTGRAY, L"Starting Higher-Half Kernel...\n");

	EFI_STATUS status;

	// === [1] GOP (Graphics Output Protocol) ===
	EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

	status = uefi_call_wrapper(systab->BootServices->LocateProtocol, 3,
				   &gop_guid, NULL, (void **)&gop);
	if (EFI_ERROR(status))
	{
		PrintFail(L"GOP not found: %r\n", status);
		return status;
	}
	PrintOk(L"GOP found\n");

	// === [2] Locate filesystem and open kernel ===
	EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_HANDLE *HandleBuffer = NULL;
	UINTN HandleCount = 0;

	status = uefi_call_wrapper(systab->BootServices->LocateHandleBuffer, 5,
				   ByProtocol, &fs_guid, NULL,
				   &HandleCount, &HandleBuffer);
	if (EFI_ERROR(status))
	{
		PrintFail(L"LocateHandleBuffer failed: %r\n", status);
		return status;
	}

	EFI_FILE_HANDLE KernelFile = NULL;
	EFI_FILE_HANDLE RootFS = NULL;

	for (UINTN i = 0; i < HandleCount; i++)
	{
		EFI_FILE_IO_INTERFACE *FileIO;
		status = uefi_call_wrapper(systab->BootServices->HandleProtocol, 3,
					   HandleBuffer[i], &fs_guid, (void **)&FileIO);
		if (EFI_ERROR(status))
			continue;

		status = uefi_call_wrapper(FileIO->OpenVolume, 2, FileIO, &RootFS);
		if (EFI_ERROR(status))
			continue;

		status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &KernelFile,
					   L"\\kernel.bin", EFI_FILE_MODE_READ, 0);
		if (!EFI_ERROR(status))
		{
			PrintOk(L"kernel.bin found on handle %u\n", i);
			break;
		}
	}

	if (HandleBuffer)
		uefi_call_wrapper(systab->BootServices->FreePool, 1, HandleBuffer);

	if (KernelFile == NULL)
	{
		PrintFail(L"kernel.bin not found\n");
		return EFI_NOT_FOUND;
	}

	// === [3] Get kernel file size ===
	EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
	EFI_FILE_INFO *KernelFileInfo = NULL;
	UINTN FileInfoSize = 0;

	status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile,
				   &FileInfoGuid, &FileInfoSize, NULL);
	if (status != EFI_BUFFER_TOO_SMALL)
	{
		PrintFail(L"GetInfo size query failed: %r\n", status);
		return status;
	}

	status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
				   FileInfoSize, (void **)&KernelFileInfo);
	if (EFI_ERROR(status))
	{
		PrintFail(L"AllocatePool failed: %r\n", status);
		return status;
	}

	status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile,
				   &FileInfoGuid, &FileInfoSize, KernelFileInfo);
	if (EFI_ERROR(status))
	{
		PrintFail(L"GetInfo failed: %r\n", status);
		return status;
	}

	UINTN kernel_size = KernelFileInfo->FileSize;
	PrintOk(L"Kernel size: %lu bytes\n", kernel_size);

	uefi_call_wrapper(BS->FreePool, 1, KernelFileInfo);

	// === [4] Allocate kernel at PHYSICAL address ===
	EFI_PHYSICAL_ADDRESS kernel_phys_addr = KERNEL_PHYS_BASE;
	UINTN kernel_pages = (kernel_size + 0xFFF) / 0x1000;

	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
				   EfiLoaderData, kernel_pages, &kernel_phys_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"AllocatePages for kernel failed: %r\n", status);
		return status;
	}

	// === [5] Read kernel to PHYSICAL address ===
	status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile,
				   &kernel_size, (void *)kernel_phys_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Read kernel failed: %r\n", status);
		return status;
	}
	PrintOk(L"Kernel loaded at physical 0x%lx\n", kernel_phys_addr);

	// DEBUG: Проверим первые байты ядра
	uint8_t *kernel_bytes = (uint8_t *)kernel_phys_addr;
	PrintDebug(L"First 16 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   kernel_bytes[0], kernel_bytes[1], kernel_bytes[2], kernel_bytes[3],
		   kernel_bytes[4], kernel_bytes[5], kernel_bytes[6], kernel_bytes[7],
		   kernel_bytes[8], kernel_bytes[9], kernel_bytes[10], kernel_bytes[11],
		   kernel_bytes[12], kernel_bytes[13], kernel_bytes[14], kernel_bytes[15]);

	uefi_call_wrapper(KernelFile->Close, 1, KernelFile);

	// === [6] Find largest memory region for heap ===
	EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
	UINTN mem_map_size = 0, map_key, desc_size;
	UINT32 desc_version;

	status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map,
				   &map_key, &desc_size, &desc_version);
	if (status != EFI_BUFFER_TOO_SMALL)
		return status;

	mem_map_size += desc_size * 10;
	status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
				   mem_map_size, (void **)&mem_map);
	if (EFI_ERROR(status))
		return status;

	status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map,
				   &map_key, &desc_size, &desc_version);
	if (EFI_ERROR(status))
		return status;

	UINTN entry_count = mem_map_size / desc_size;

	EFI_MEMORY_DESCRIPTOR *largest = NULL;
	UINT64 largest_size = 0;

	PrintDebug(L"Scanning memory regions...\n");
	for (UINTN i = 0; i < entry_count; i++)
	{
		EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mem_map + (i * desc_size));

		if (desc->Type == EfiConventionalMemory)
		{
			UINT64 size = desc->NumberOfPages * EFI_PAGE_SIZE;
			PrintDebug(L"  Region %u: 0x%lx - 0x%lx (%lu MB)\n",
				   i, desc->PhysicalStart,
				   desc->PhysicalStart + size,
				   size / (1024 * 1024));

			if (size > largest_size)
			{
				largest_size = size;
				largest = desc;
			}
		}
	}

	if (!largest || largest_size < 64 * 1024 * 1024)
	{
		PrintFail(L"No large memory region found (need >=64MB)\n");
		return EFI_OUT_OF_RESOURCES;
	}

	PrintOk(L"Largest region: 0x%lx - 0x%lx (%lu MB)\n",
		largest->PhysicalStart,
		largest->PhysicalStart + largest_size,
		largest_size / (1024 * 1024));

	// === [7] Reserve heap ===
	EFI_PHYSICAL_ADDRESS heap_phys = largest->PhysicalStart;
	UINT64 heap_size = largest->NumberOfPages * 0x1000;
	UINTN heap_pages = largest->NumberOfPages;

	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
				   EfiLoaderData, heap_pages, &heap_phys);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Failed to reserve heap: %r\n", status);
		return status;
	}
	PrintOk(L"Heap reserved at 0x%lx (%lu MB)\n", heap_phys, heap_size / (1024 * 1024));

	// === [8] Create page tables with higher-half mapping ===
	PrintInfo(L"Creating higher-half page tables...\n");

	// Allocate PML4
	EFI_PHYSICAL_ADDRESS pml4_addr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
				   EfiLoaderData, 1, &pml4_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Failed to allocate PML4: %r\n", status);
		return status;
	}

	UINT64 *pml4 = (UINT64 *)pml4_addr;
	for (int i = 0; i < 512; i++)
		pml4[i] = 0;

	// === Create mappings ===

	// 1. Identity map first 4GB
	PrintInfo(L"Creating identity mapping (0-4GB)...\n");
	for (UINT64 phys = 0; phys < 0x100000000ULL; phys += 0x200000)
	{
		UINT64 pml4_idx = (phys >> 39) & 0x1FF;
		UINT64 pdpt_idx = (phys >> 30) & 0x1FF;
		UINT64 pd_idx = (phys >> 21) & 0x1FF;

		if (!(pml4[pml4_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pdpt_addr = 0;
			status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
						   EfiLoaderData, 1, &pdpt_addr);
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
			status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
						   EfiLoaderData, 1, &pd_addr);
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
	PrintInfo(L"Creating higher-half mapping (0-4GB)...\n");

	// Выделяем отдельный PDPT для higher-half (PML4[511])
	EFI_PHYSICAL_ADDRESS hh_pdpt_addr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
				   EfiLoaderData, 1, &hh_pdpt_addr);
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
			status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, &pd_addr);
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
	PrintInfo(L"Mapping MMIO region (0xF0000000-0xFFFFFFFF)...\n");
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

	PrintOk(L"Page tables created at 0x%lx\n", pml4_addr);

	// DEBUG: Verify page table mapping for kernel entry
	UINT64 test_virt = PHYS_TO_VIRT(KERNEL_PHYS_BASE);
	UINT64 pml4_idx = (test_virt >> 39) & 0x1FF;
	UINT64 pdpt_idx = (test_virt >> 30) & 0x1FF;
	UINT64 pd_idx = (test_virt >> 21) & 0x1FF;

	PrintDebug(L"Verifying mapping for 0x%lx:\n", test_virt);
	PrintDebug(L"  Indices: PML4[%u] PDPT[%u] PD[%u]\n", pml4_idx, pdpt_idx, pd_idx);
	PrintDebug(L"  PML4[%u] = 0x%lx\n", pml4_idx, pml4[pml4_idx]);

	if (pml4[pml4_idx] & 0x1)
	{
		UINT64 *pdpt = (UINT64 *)(pml4[pml4_idx] & ~0xFFFULL);
		PrintDebug(L"  PDPT[%u] = 0x%lx\n", pdpt_idx, pdpt[pdpt_idx]);

		if (pdpt[pdpt_idx] & 0x1)
		{
			UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFULL);
			PrintDebug(L"  PD[%u] = 0x%lx\n", pd_idx, pd[pd_idx]);

			if (pd[pd_idx] & 0x1)
			{
				UINT64 phys_mapped = pd[pd_idx] & ~0x1FFFFFULL;
				PrintOk(L"  Maps to physical: 0x%lx (expected 0x%lx)\n",
					phys_mapped, KERNEL_PHYS_BASE & ~0x1FFFFFULL);
			}
			else
			{
				PrintFail(L"  PD entry not present!\n");
			}
		}
		else
		{
			PrintFail(L"  PDPT entry not present!\n");
		}
	}
	else
	{
		PrintFail(L"  PML4 entry not present!\n");
	}

	uefi_call_wrapper(RootFS->Close, 1, RootFS);
	uefi_call_wrapper(BS->FreePool, 1, mem_map);

	// === [9] Allocate and FILL boot info ===
	EFI_PHYSICAL_ADDRESS boot_info_addr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
				   EfiLoaderData, 1, &boot_info_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"AllocatePages for boot_info failed: %r\n", status);
		return status;
	}

	// === [10] allocate and fill rsdp ===
	// === [10] Find RSDP ===
	void *rsdp = find_rsdp(systab);
	if (!rsdp)
	{
		PrintFail(L"RSDP not found\n");
		return EFI_NOT_FOUND;
	}
	PrintOk(L"RSDP at physical: 0x%lx\n", rsdp);

	char *sig = (char *)rsdp;
	Print(L"RSDP signature bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	      sig[0], sig[1], sig[2], sig[3], sig[4], sig[5], sig[6], sig[7]);

	// Check for "RSD PTR "
	if (sig[0] != 'R' || sig[1] != 'S' || sig[2] != 'D' || sig[3] != ' ' ||
	    sig[4] != 'P' || sig[5] != 'T' || sig[6] != 'R' || sig[7] != ' ')
	{
		PrintFail(L"RSDP signature invalid IN BOOTLOADER!\n");
		Print(L"Expected: 'RSD PTR ', Got: '");
		for (int i = 0; i < 8; i++)
			Print(L"%c", sig[i]);
		Print(L"'\n");
		return EFI_INVALID_PARAMETER;
	}

	PrintOk(L"RSDP signature verified in bootloader\n");

	// === [11] fill boot info ===

	BootInfo *boot_info = (BootInfo *)boot_info_addr;
	framebuffer_info_t *fb_info = &boot_info->framebuffer_data;
	ram_info_t *ram_info = &boot_info->memory_data;

	boot_info->rsdp = rsdp;
	PrintOk(L"RSDP at physical: 0x%lx\n", rsdp);

	fb_info->base = (void *)gop->Mode->FrameBufferBase;
	fb_info->width = gop->Mode->Info->HorizontalResolution;
	fb_info->height = gop->Mode->Info->VerticalResolution;
	fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
	fb_info->bpp = 32;

	ram_info->heap_start = heap_phys;
	ram_info->heap_size = heap_size;
	ram_info->pml4_phys = pml4_addr;

	boot_info->framebuffer = fb_info;
	boot_info->memory_map = ram_info;

	PrintOk(L"Framebuffer: %ux%u @ 0x%lx\n",
		fb_info->width, fb_info->height, (UINT64)fb_info->base);
	PrintOk(L"Boot info at physical: 0x%lx\n", boot_info_addr);

	// === [9.5] Allocate stack BEFORE ExitBootServices ===
	EFI_PHYSICAL_ADDRESS stack_phys = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
				   EfiLoaderData, 16, &stack_phys);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Failed to allocate stack: %r\n", status);
		return status;
	}

	// Stack grows down, so point to top
	UINT64 stack_top = stack_phys + (16 * 0x1000);
	UINT64 stack_virt = PHYS_TO_VIRT(stack_top);
	PrintOk(L"Stack allocated at physical: 0x%lx (top: 0x%lx)\n", stack_phys, stack_top);

	// Вычисляем виртуальные адреса
	UINT64 kernel_virt_entry = PHYS_TO_VIRT(KERNEL_PHYS_BASE);
	UINT64 boot_info_virt = PHYS_TO_VIRT(boot_info_addr);

	PrintInfo(L"Kernel virtual entry: 0x%lx\n", kernel_virt_entry);
	PrintInfo(L"Boot info virtual: 0x%lx\n", boot_info_virt);
	PrintInfo(L"Stack virtual: 0x%lx\n", stack_virt);

	PrintInfo(L"Jump to kernel\n");
	ClearConsole();

	// === [10] ExitBootServices ===
	mem_map = NULL;
	mem_map_size = 0;

	status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map,
				   &map_key, &desc_size, &desc_version);
	if (status != EFI_BUFFER_TOO_SMALL)
		return status;

	mem_map_size += desc_size * 10;
	status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
				   mem_map_size, (void **)&mem_map);
	if (EFI_ERROR(status))
		return status;

	status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map,
				   &map_key, &desc_size, &desc_version);
	if (EFI_ERROR(status))
		return status;

	status = uefi_call_wrapper(BS->ExitBootServices, 2, image, map_key);
	if (EFI_ERROR(status))
	{
		uefi_call_wrapper(BS->FreePool, 1, mem_map);
		return status;
	}

	// === POINT OF NO RETURN ===

	// === [11] Switch to new page tables and TEST ===
	set_cr3(pml4_addr);

	// === [12] Setup stack and jump ===
	asm volatile(
	    "mov %[stack], %%rsp\n" // установить стек
	    "xor %%rbp, %%rbp\n"    // просто обнулить base pointer, если нужно
	    "xor %%rax, %%rax\n"
	    "xor %%rbx, %%rbx\n"
	    "xor %%rcx, %%rcx\n"
	    "xor %%rdx, %%rdx\n"
	    "xor %%rsi, %%rsi\n"
	    "mov %[boot], %%rdi\n"  // аргумент ядра
	    "mov %[entry], %%r11\n" // адрес entry
	    "jmp *%%r11\n"	    // прыжок в ядро
	    :
	    : [stack] "r"(stack_virt),
	      [entry] "r"(kernel_virt_entry),
	      [boot] "r"(boot_info_virt)
	    : "memory", "r11", "rax", "rbx", "rcx", "rdx", "rsi", "rdi");

	// Should never reach here
	while (1)
		asm volatile("hlt");

	return EFI_SUCCESS;
}
