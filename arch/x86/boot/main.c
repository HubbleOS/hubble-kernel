#include <efi.h>
#include <efilib.h>
#include <stdint.h>

#include <bootinfo/bootinfo.h>

#include "globals.h"
#include "console.h"

// boot/paging.c
#include <efi.h>
#include <efilib.h>

#define PAGE_SIZE 0x1000
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_PS (1ULL << 7) // Page Size (2MB pages)

typedef struct
{
	UINT64 entries[512];
} __attribute__((aligned(4096))) PageTable;

// Allocate page table using UEFI
static PageTable *alloc_page_table(EFI_BOOT_SERVICES *BS)
{
	EFI_PHYSICAL_ADDRESS addr = 0;
	EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4,
					      AllocateAnyPages,
					      EfiLoaderData,
					      1, // 1 page
					      &addr);

	if (EFI_ERROR(status))
		return NULL;

	PageTable *table = (PageTable *)addr;

	// Zero the table
	for (int i = 0; i < 512; i++)
		table->entries[i] = 0;

	return table;
}

// Setup identity paging for first 4GB using 2MB pages
EFI_PHYSICAL_ADDRESS setup_paging(EFI_BOOT_SERVICES *BS)
{
	// Allocate PML4
	PageTable *pml4 = alloc_page_table(BS);
	if (!pml4)
		return 0;

	// Allocate PDPT (Page Directory Pointer Table)
	PageTable *pdpt = alloc_page_table(BS);
	if (!pdpt)
		return 0;

	// Link PML4[0] -> PDPT
	pml4->entries[0] = (UINT64)pdpt | PTE_PRESENT | PTE_WRITABLE;

	// For each 1GB in first 4GB
	for (int i = 0; i < 4; i++)
	{
		// Allocate Page Directory
		PageTable *pd = alloc_page_table(BS);
		if (!pd)
			return 0;

		// Link PDPT[i] -> PD
		pdpt->entries[i] = (UINT64)pd | PTE_PRESENT | PTE_WRITABLE;

		// Map 512 * 2MB = 1GB using 2MB pages
		for (int j = 0; j < 512; j++)
		{
			UINT64 phys_addr = (UINT64)i * 0x40000000ULL + (UINT64)j * 0x200000ULL;
			pd->entries[j] = phys_addr | PTE_PRESENT | PTE_WRITABLE | PTE_PS;
		}
	}

	// CRITICAL: Recursive mapping - map PML4 to itself at index 511
	pml4->entries[511] = (UINT64)pml4 | PTE_PRESENT | PTE_WRITABLE;

	return (EFI_PHYSICAL_ADDRESS)pml4;
}

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

	// === [6] We divide the region BEFORE selection ===

	// Getting a memory map
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

	// We're looking for the largest EfiConventionalMemory region
	EFI_MEMORY_DESCRIPTOR *largest = NULL;
	UINT64 largest_size = 0;

	PrintDebug(L"Scanning memory regions...\n");
	for (UINTN i = 0; i < entry_count; i++)
	{
		EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mem_map + (i * desc_size));

		if (desc->Type == EfiConventionalMemory)
		{
			UINT64 size = desc->NumberOfPages * EFI_PAGE_SIZE;
			PrintDebug(L"Scanning  Region %u: 0x%lx - 0x%lx (%lu MB)\n",
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

	PrintOk(L"KernelLargest region: 0x%lx - 0x%lx (%lu MB)\n",
		largest->PhysicalStart,
		largest->PhysicalStart + largest_size,
		largest_size / (1024 * 1024));

	// CRITICAL: Open the ramdisk file to find out the size
	EFI_FILE_HANDLE RamdiskFile = NULL;
	UINTN ramdisk_size = 0;

	status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &RamdiskFile,
				   L"\\ramdisk.img", EFI_FILE_MODE_READ, 0);

	if (!EFI_ERROR(status))
	{
		EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
		EFI_FILE_INFO *RamdiskFileInfo = NULL;
		UINTN FileInfoSize = 0;

		status = uefi_call_wrapper(RamdiskFile->GetInfo, 4, RamdiskFile,
					   &FileInfoGuid, &FileInfoSize, NULL);
		if (status == EFI_BUFFER_TOO_SMALL)
		{
			status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
						   FileInfoSize, (void **)&RamdiskFileInfo);
			if (!EFI_ERROR(status))
			{
				status = uefi_call_wrapper(RamdiskFile->GetInfo, 4, RamdiskFile,
							   &FileInfoGuid, &FileInfoSize, RamdiskFileInfo);
				if (!EFI_ERROR(status))
				{
					ramdisk_size = RamdiskFileInfo->FileSize;
				}
				uefi_call_wrapper(BS->FreePool, 1, RamdiskFileInfo);
			}
		}
	}

	if (ramdisk_size == 0)
	{
		PrintWarn(L"ramdisk.img not found or empty\n");
	}
	else
	{
		PrintOk(L"KernelRamdisk size: %lu MB\n", ramdisk_size / (1024 * 1024));
	}

	// We are planning to divide the largest region:
	// [RAMDISK][1MB buffer][ HEAP (minimum 32MB) ]

	UINTN ramdisk_pages = (ramdisk_size + 0xFFF) / 0x1000;
	UINTN buffer_pages = 256;			    // 1MB buffer
	UINTN min_heap_pages = (32 * 1024 * 1024) / 0x1000; // 32MB minimum

	UINTN required_pages = ramdisk_pages + buffer_pages + min_heap_pages;

	if (largest->NumberOfPages < required_pages)
	{
		PrintFail(L"Largest region too small for ramdisk + heap\n");
		PrintFail(L"Need %lu pages, have %lu pages\n",
			  required_pages, largest->NumberOfPages);
		PrintFail(L"Try: 1) Reduce ramdisk size, or 2) Increase VM memory\n");
		return EFI_OUT_OF_RESOURCES;
	}

	// Calculating addresses
	EFI_PHYSICAL_ADDRESS ramdisk_addr = largest->PhysicalStart;
	EFI_PHYSICAL_ADDRESS heap_addr = ramdisk_addr +
					 (ramdisk_pages + buffer_pages) * 0x1000;
	UINT64 heap_size = (largest->NumberOfPages - ramdisk_pages - buffer_pages) * 0x1000;

	PrintInfo(L"Kernel Memory layout:\n");
	PrintInfo(L"  Ramdisk: 0x%lx - 0x%lx (%lu MB)\n",
		  ramdisk_addr,
		  ramdisk_addr + ramdisk_pages * 0x1000,
		  (ramdisk_pages * 0x1000) / (1024 * 1024));
	PrintInfo(L"  Buffer:  0x%lx - 0x%lx (1 MB)\n",
		  ramdisk_addr + ramdisk_pages * 0x1000,
		  heap_addr);
	PrintInfo(L"  Heap:    0x%lx - 0x%lx (%lu MB)\n",
		  heap_addr,
		  heap_addr + heap_size,
		  heap_size / (1024 * 1024));

	// IMPORTANT: First we reserve HEAP (from the end of the region)
	EFI_PHYSICAL_ADDRESS heap_phys = heap_addr;
	UINTN heap_pages = heap_size / 0x1000;

	status = uefi_call_wrapper(BS->AllocatePages, 4,
				   AllocateAddress,
				   EfiLoaderData,
				   heap_pages,
				   &heap_phys);
	if (EFI_ERROR(status))
	{
		PrintFail(L"Failed to reserve heap: %r\n", status);
		return status;
	}
	PrintOk(L"KernelHeap reserved at 0x%lx\n", heap_phys);

	// Now select the ramdisk (from the beginning of the region)
	void *ramdisk_ptr = NULL;
	if (ramdisk_size > 0 && RamdiskFile)
	{
		EFI_PHYSICAL_ADDRESS ramdisk_phys = ramdisk_addr;

		status = uefi_call_wrapper(BS->AllocatePages, 4,
					   AllocateAddress,
					   EfiLoaderData,
					   ramdisk_pages,
					   &ramdisk_phys);
		if (!EFI_ERROR(status))
		{
			ramdisk_ptr = (void *)ramdisk_phys;
			UINTN read_size = ramdisk_size;

			status = uefi_call_wrapper(RamdiskFile->Read, 3,
						   RamdiskFile,
						   &read_size,
						   ramdisk_ptr);
			if (!EFI_ERROR(status))
			{
				PrintOk(L"KernelRamdisk loaded at 0x%lx\n", (UINT64)ramdisk_ptr);
			}
			else
			{
				PrintFail(L"Failed to read ramdisk: %r\n", status);
				ramdisk_ptr = NULL;
				ramdisk_size = 0;
			}
		}
		else
		{
			PrintFail(L"Failed to allocate ramdisk: %r\n", status);
			ramdisk_ptr = NULL;
			ramdisk_size = 0;
		}
	}

	if (RamdiskFile)
		uefi_call_wrapper(RamdiskFile->Close, 1, RamdiskFile);

	uefi_call_wrapper(RootFS->Close, 1, RootFS);
	uefi_call_wrapper(BS->FreePool, 1, mem_map);

	// === [7] Allocate boot info structures ===
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
	ramdisk_info_t *ramdisk_info = (ramdisk_info_t *)((uint8_t *)ram_info + sizeof(ram_info_t));

	// === [8] Fill framebuffer info ===
	fb_info->base = (void *)gop->Mode->FrameBufferBase;
	fb_info->width = gop->Mode->Info->HorizontalResolution;
	fb_info->height = gop->Mode->Info->VerticalResolution;
	fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
	fb_info->bpp = 32;

	PrintOk(L"KernelFramebuffer: %ux%u @ 0x%lx\n",
		fb_info->width, fb_info->height, (UINT64)fb_info->base);

	// === [9] Fill ramdisk info ===
	ramdisk_info->ramdisk_base = ramdisk_ptr;
	ramdisk_info->ramdisk_size = ramdisk_size;

	// === [10] Fill heap info ===
	ram_info->heap_start = heap_phys;
	ram_info->heap_size = heap_size;

	PrintOk(L"KernelFinal heap: 0x%lx - 0x%lx (%lu MB)\n",
		ram_info->heap_start,
		ram_info->heap_start + ram_info->heap_size,
		ram_info->heap_size / (1024 * 1024));

	PrintInfo(L"Jumping to kernel at 0x%lx\n", (UINT64)kernel_addr);
	ClearConsole();

	PrintInfo(L"Setting up page tables...\n");
	EFI_PHYSICAL_ADDRESS new_cr3 = setup_paging(BS);
	if (new_cr3 == 0)
	{
		PrintFail(L"Failed to setup page tables\n");
		return EFI_OUT_OF_RESOURCES;
	}
	PrintOk(L"Page tables at 0x%lx\n", new_cr3);

	// Load new CR3
	__asm__ volatile("mov %0, %%cr3" : : "r"(new_cr3) : "memory");
	PrintOk(L"CR3 loaded\n");

	// === [11] Prepare BootInfo pointer ===
	BootInfo boot_info;
	boot_info.framebuffer = fb_info;
	boot_info.memory_map = ram_info;
	boot_info.disk_info = ramdisk_info;

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
