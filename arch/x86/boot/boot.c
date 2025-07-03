#include <efi.h>
#include <efilib.h>
#include <stdint.h>

#include "utils/framebuffer.h"

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);
    Print(L"[1] Bootloader started\n");

    // === [2] GPU ===
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status;

    status = uefi_call_wrapper(systab->BootServices->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"[2] GOP not found: %r\n", status);
        return status;
    }

    // === [3] Read kernel ===
    void *kernel_addr = (void *)0x100000;
    UINTN kernel_size = 1024 * 1024;

    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_FILE_IO_INTERFACE *FileIO;
    EFI_FILE_HANDLE RootFS, KernelFile;

    status = uefi_call_wrapper(systab->BootServices->LocateProtocol, 3, &fs_guid, NULL, (void **)&FileIO);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(FileIO->OpenVolume, 2, FileIO, &RootFS);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &KernelFile, L"kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &kernel_size, kernel_addr);
    if (EFI_ERROR(status))
        return status;

    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    uefi_call_wrapper(RootFS->Close, 1, RootFS);

    // === [4] Виділення місця для framebuffer_info ===
    framebuffer_info_t *fb_info = (framebuffer_info_t *)((uint8_t *)kernel_addr + kernel_size + 0x1000);

    fb_info->base = (void *)gop->Mode->FrameBufferBase;
    fb_info->width = gop->Mode->Info->HorizontalResolution;
    fb_info->height = gop->Mode->Info->VerticalResolution;
    fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    fb_info->bpp = 32;

    // === [5] Find largest EfiConventionalMemory region for heap ===
    EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
    UINTN mem_map_size = 0, map_key, desc_size;
    UINT32 desc_version;

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL)
        return status;

    mem_map_size += desc_size * 10;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mem_map_size, (void **)&mem_map);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status))
        return status;

    EFI_MEMORY_DESCRIPTOR *best = NULL;
    UINTN entry_count = mem_map_size / desc_size;
    for (UINTN i = 0; i < entry_count; i++)
    {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mem_map + (i * desc_size));
        if (desc->Type == EfiConventionalMemory)
        {
            if (!best || desc->NumberOfPages > best->NumberOfPages)
                best = desc;
        }
    }

    fb_info->heap_start = best->PhysicalStart;
    fb_info->heap_size = best->NumberOfPages * EFI_PAGE_SIZE;

    Print(L"[Debug] heap_start=%lx, heap_size=%lx\n", fb_info->heap_start, fb_info->heap_size);

    // === [6] Final memory map & ExitBootServices ===
    mem_map_size = 0;
    mem_map = NULL;

    // 1. Перший виклик – отримати розмір
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL)
        return status;

    // 2. Запас
    mem_map_size += desc_size * 10;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mem_map_size, (void **)&mem_map);
    if (EFI_ERROR(status))
        return status;

    // 3. Другий виклик – вже з буфером
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status))
        return status;

    // --- НІЧОГО БІЛЬШЕ НЕ ВИКЛИКАТИ ПІСЛЯ ЦЬОГО! ---
    // ExitBootServices МАЄ йти одразу після GetMemoryMap
    status = uefi_call_wrapper(BS->ExitBootServices, 2, image, map_key);
    if (EFI_ERROR(status))
    {
        Print(L"ExitBootServices failed: %r\n", status);
        return status;
    }

    // === [7] Передача керування ядру ===
    void (*kernel_entry)(framebuffer_info_t *) = (void *)kernel_addr;
    kernel_entry(fb_info);

    return EFI_SUCCESS;
}
