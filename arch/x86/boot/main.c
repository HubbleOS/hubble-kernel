#include <efi.h>
#include <efilib.h>
#include <stdint.h>

#include <bootinfo/bootinfo.h>

#include "globals.h"
#include "console.h"

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	g_image = image;
	g_systab = systab;

	InitializeLib(image, systab);
	ClearConsole();

	PrintMessage(L"BOOT", EFI_LIGHTGRAY, L"Starting...\n");

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

	// === [4] Allocate kernel at fixed address ===
	EFI_PHYSICAL_ADDRESS kernel_addr = 0x100000;
	UINTN kernel_pages = (kernel_size + 0xFFF) / 0x1000;

	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
				   EfiLoaderData, kernel_pages, &kernel_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"AllocatePages for kernel failed: %r\n", status);
		return status;
	}

	// === [5] Read kernel ===
	status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile,
				   &kernel_size, (void *)kernel_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Read kernel failed: %r\n", status);
		return status;
	}
	PrintOk(L"Kernel loaded at 0x%lx\n", kernel_addr);
	uefi_call_wrapper(KernelFile->Close, 1, KernelFile);

	// === [6] Find largest memory region ===
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

	// === [8] Simplified scheme - only heap ===
	UINTN min_heap_pages = (64 * 1024 * 1024) / 0x1000;
	UINTN required_pages = min_heap_pages;

	if (largest->NumberOfPages < required_pages)
	{
		PrintFail(L"Largest region too small\n");
		return EFI_OUT_OF_RESOURCES;
	}

	// Layout: [heap - the rest]
	EFI_PHYSICAL_ADDRESS heap_addr = largest->PhysicalStart;
	UINT64 heap_size = (largest->NumberOfPages) * 0x1000;

	PrintInfo(L"Memory layout:\n");
	PrintInfo(L"  Heap:    0x%lx - 0x%lx (%lu MB)\n",
		  heap_addr, heap_addr + heap_size,
		  heap_size / (1024 * 1024));

	// === [9] Allocate regions ===

	// Reserve heap
	EFI_PHYSICAL_ADDRESS heap_phys = heap_addr;
	UINTN heap_pages = heap_size / 0x1000;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
				   EfiLoaderData, heap_pages, &heap_phys);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Failed to reserve heap: %r\n", status);
		return status;
	}
	PrintOk(L"Heap reserved at 0x%lx (%lu MB)\n", heap_phys, heap_size / (1024 * 1024));

	// Исправленная версия создания page tables в загрузчике

	// === [10] Create identity mapping for kernel ===
	PrintInfo(L"Creating identity page tables...\n");

	// Выделяем PML4
	EFI_PHYSICAL_ADDRESS pml4_addr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
				   EfiLoaderData, 1, &pml4_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Failed to allocate PML4: %r\n", status);
		return status;
	}

	// ⚠️ ВАЖНО: В UEFI физ. адрес == виртуальный (identity mapped)
	UINT64 *pml4 = (UINT64 *)pml4_addr;

	// Очищаем PML4
	for (int i = 0; i < 512; i++)
	{
		pml4[i] = 0;
	}

	// 1. Мапим первые 4GB (kernel code, видеопамять и т.д.)
	for (UINT64 phys = 0; phys < 0x100000000ULL; phys += 0x200000)
	{
		UINT64 pml4_idx = (phys >> 39) & 0x1FF;
		UINT64 pdpt_idx = (phys >> 30) & 0x1FF;
		UINT64 pd_idx = (phys >> 21) & 0x1FF;

		// Создаем PDPT если нужно
		if (!(pml4[pml4_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pdpt_addr = 0;
			status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
						   EfiLoaderData, 1, &pdpt_addr);
			if (EFI_ERROR(status))
			{
				PrintFail(L"Failed to allocate PDPT\n");
				return status;
			}

			UINT64 *pdpt = (UINT64 *)pdpt_addr;
			for (int i = 0; i < 512; i++)
				pdpt[i] = 0;

			pml4[pml4_idx] = pdpt_addr | 0x3; // Present + Write
		}

		UINT64 *pdpt = (UINT64 *)(pml4[pml4_idx] & ~0xFFFULL);

		// Создаем PD если нужно
		if (!(pdpt[pdpt_idx] & 0x1))
		{
			EFI_PHYSICAL_ADDRESS pd_addr = 0;
			status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
						   EfiLoaderData, 1, &pd_addr);
			if (EFI_ERROR(status))
			{
				PrintFail(L"Failed to allocate PD\n");
				return status;
			}

			UINT64 *pd = (UINT64 *)pd_addr;
			for (int i = 0; i < 512; i++)
				pd[i] = 0;

			pdpt[pdpt_idx] = pd_addr | 0x3; // Present + Write
		}

		UINT64 *pd = (UINT64 *)(pdpt[pdpt_idx] & ~0xFFFULL);

		// Мапим 2MB huge page
		pd[pd_idx] = phys | 0x83; // Present + Write + Huge (2MB)
	}

	// 2. КРИТИЧНО: Мапим всю heap область (с выравниванием вниз/вверх)
	UINT64 heap_start_aligned = heap_phys & ~0x1FFFFFULL;			     // Round down to 2MB
	UINT64 heap_end_aligned = (heap_phys + heap_size + 0x1FFFFF) & ~0x1FFFFFULL; // Round up

	PrintInfo(L"Mapping heap range: 0x%lx - 0x%lx\n", heap_start_aligned, heap_end_aligned);

	for (UINT64 phys = heap_start_aligned; phys < heap_end_aligned; phys += 0x200000)
	{
		UINT64 pml4_idx = (phys >> 39) & 0x1FF;
		UINT64 pdpt_idx = (phys >> 30) & 0x1FF;
		UINT64 pd_idx = (phys >> 21) & 0x1FF;

		// Создаем PDPT если нужно
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

		// Создаем PD если нужно
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
		pd[pd_idx] = phys | 0x83; // Present + Write + Huge (2MB)
	}

	// 3. Добавляем рекурсивный mapping (511-й entry PML4 указывает на себя)
	pml4[511] = pml4_addr | 0x3; // Present + Write

	PrintOk(L"Identity page tables created at 0x%lx\n", pml4_addr);

	// Устанавливаем новую PML4
	asm volatile("mov %0, %%cr3" : : "r"(pml4_addr) : "memory");
	PrintOk(L"Switched to new page tables\n");

	uefi_call_wrapper(RootFS->Close, 1, RootFS);
	uefi_call_wrapper(BS->FreePool, 1, mem_map);

	// === [11] Allocate boot info structures ===
	EFI_PHYSICAL_ADDRESS boot_info_addr = 0;
	UINTN boot_info_pages = 1;

	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
				   EfiLoaderData, boot_info_pages, &boot_info_addr);
	if (EFI_ERROR(status))
	{
		PrintFail(L"AllocatePages for boot_info failed: %r\n", status);
		return status;
	}

	framebuffer_info_t *fb_info = (framebuffer_info_t *)boot_info_addr;
	ram_info_t *ram_info = (ram_info_t *)((uint8_t *)fb_info + sizeof(framebuffer_info_t));

	fb_info->base = (void *)gop->Mode->FrameBufferBase;
	fb_info->width = gop->Mode->Info->HorizontalResolution;
	fb_info->height = gop->Mode->Info->VerticalResolution;
	fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
	fb_info->bpp = 32;

	ram_info->heap_start = heap_phys;
	ram_info->heap_size = heap_size;

	// Зберігаємо адресу PML4 в boot info
	ram_info->pml4_phys = pml4_addr;

	PrintOk(L"Framebuffer: %ux%u @ 0x%lx\n",
		fb_info->width, fb_info->height, (UINT64)fb_info->base);

	PrintInfo(L"Jumping to kernel at 0x%lx\n", (UINT64)kernel_addr);
	ClearConsole();

	BootInfo boot_info;
	boot_info.framebuffer = fb_info;
	boot_info.memory_map = ram_info;

	// === [12] Final ExitBootServices ===
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
		return status;

	// === [13] Jump to kernel ===
	void (*kernel_entry)(BootInfo *) = (void *)kernel_addr;
	kernel_entry(&boot_info);

	while (1)
		asm("hlt");

	return EFI_SUCCESS;
}
