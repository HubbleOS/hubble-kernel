#include <efi.h>
#include <efilib.h>
#include <stdint.h>

#include "utils/framebuffer.h"

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);
    Print(L"[1] Bootloader started debug\n");

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

    UINTN kernel_size = 1024 * 1024;

    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_FILE_IO_INTERFACE *FileIO;
    EFI_FILE_HANDLE RootFS;

    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    EFI_STATUS Status;

    // Знайти усі хендли, які підтримують EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
    Status = uefi_call_wrapper(systab->BootServices->LocateHandleBuffer, 5,
                               ByProtocol,
                               &fs_guid,
                               NULL,
                               &HandleCount,
                               &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        Print(L"LocateHandleBuffer failed: %r\n", Status);
        return Status;
    }

    EFI_FILE_HANDLE KernelFile = NULL;

    for (UINTN i = 0; i < HandleCount; i++)
    {
        EFI_FILE_IO_INTERFACE *FileIO;

        Status = uefi_call_wrapper(systab->BootServices->HandleProtocol, 3,
                                   HandleBuffer[i],
                                   &fs_guid,
                                   (void **)&FileIO);
        if (EFI_ERROR(Status))
            continue;

        Status = uefi_call_wrapper(FileIO->OpenVolume, 2, FileIO, &RootFS);
        if (EFI_ERROR(Status))
            continue;

        EFI_FILE_HANDLE TempKernelFile;
        Status = uefi_call_wrapper(RootFS->Open, 5,
                                   RootFS,
                                   &TempKernelFile,
                                   L"\\kernel.bin",
                                   EFI_FILE_MODE_READ,
                                   0);
        if (EFI_ERROR(Status))
        {
            Print(L"Failed to open kernel.bin on handle %u: %r\n", i, Status);
            continue;
        }
        else
        {
            Print(L"Kernel file opened successfully on handle %u.\n", i);
            KernelFile = TempKernelFile;
            RootFS = RootFS; // Збережи, якщо потрібно працювати з RootFS пізніше
            break;           // Вийти з циклу, бо файл знайдено
        }
    }

    // Після циклу перевір KernelFile
    if (KernelFile == NULL)
    {
        Print(L"kernel.bin не знайдено на жодному з розділів!\n");
        return EFI_NOT_FOUND;
    }

    // Далі працюй з KernelFile, RootFS, тощо...

    // Очистити памʼять
    Print(L"FreeHandleBuffer\n");
    if (HandleBuffer)
        uefi_call_wrapper(systab->BootServices->FreePool, 1, HandleBuffer);

    EFI_FILE_INFO *KernelFileInfo = NULL;
    UINTN FileInfoSize = sizeof(UINT8);
    EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
    UINT8 dummy_buffer[1];

    Print(L"KernelFile = %p\n", KernelFile);
    if (KernelFile == NULL)
    {
        Print(L"KernelFile is NULL!\n");
        return EFI_NOT_FOUND;
    }
    if (KernelFile->GetInfo == NULL)
    {
        Print(L"KernelFile->GetInfo is NULL!\n");
        return EFI_NOT_FOUND;
    }
    if (KernelFile->Read == NULL)
    {
        Print(L"KernelFile->Read is NULL!\n");
        return EFI_NOT_FOUND;
    }

    Print(L"GetInfo (size query)\n");
    status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &FileInfoGuid, &FileInfoSize, dummy_buffer);
    if (status != EFI_BUFFER_TOO_SMALL)
    {
        Print(L"Unexpected error from GetInfo (size query): %r\n", status);
        return status;
    }
    Print(L"GetInfo (size query) done\n");
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, FileInfoSize, (void **)&KernelFileInfo);
    if (EFI_ERROR(status))
    {
        Print(L"AllocatePool failed: %r\n", status);
        return status;
    }
    Print(L"KernelFile: %p, GetInfo ptr: %p\n", KernelFile, KernelFile->GetInfo);

    status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &FileInfoGuid, &FileInfoSize, KernelFileInfo);
    if (EFI_ERROR(status))
    {
        Print(L"GetInfo failed: %r\n", status);
        return status;
    }

    kernel_size = KernelFileInfo->FileSize;

    Print(L"Kernel file size: %lu\n", kernel_size);

    EFI_PHYSICAL_ADDRESS kernel_addr = 0x100000;
    UINTN pages = (kernel_size + 0xFFF) / 0x1000; // округлення до сторінок

    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &kernel_addr);
    if (EFI_ERROR(status))
    {
        Print(L"AllocatePages failed: %r\n", status);
        return status;
    }

    Print(L"Read kernel\n");
    status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &kernel_size, kernel_addr);
    if (EFI_ERROR(status))
    {
        Print(L"Read failed: %r\n", status);
        return status;
    }
    Print(L"Read kernel done\n");
    EFI_FILE_PROTOCOL *RamdiskFile;
    UINTN ramdisk_size;
    void *ramdisk_addr;

    // Відкрити файл ramdisk.img
    Print(L"Open ramdisk\n");
    status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &RamdiskFile, L"\\ramdisk.img", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Print(L"Open failed ramdisk: %r\n", status);
        return status;
    }
    Print(L"Open ramdisk done\n");
    // Отримати розмір файлу (через FileInfo)

    EFI_FILE_INFO *FileInfo = NULL;

    // 1. Дізнаємось розмір буфера
    status = uefi_call_wrapper(RamdiskFile->GetInfo, 4, RamdiskFile, &FileInfoGuid, &FileInfoSize, NULL);
    if (status != EFI_BUFFER_TOO_SMALL)
    {
        Print(L"Unexpected error from GetInfo size query: %r\n", status);
        return status;
    }

    // 2. Виділяємо пам'ять під буфер
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, FileInfoSize, (void **)&FileInfo);
    if (EFI_ERROR(status))
    {
        Print(L"AllocatePool failed: %r\n", status);
        return status;
    }

    // 3. Отримуємо інфо
    status = uefi_call_wrapper(RamdiskFile->GetInfo, 4, RamdiskFile, &FileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(status))
    {
        Print(L"GetInfo failed: %r\n", status);
        uefi_call_wrapper(BS->FreePool, 1, FileInfo);
        return status;
    }

    Print(L"Ramdisk size: %lu\n", FileInfo->FileSize);

    ramdisk_size = FileInfo->FileSize;

    // Виділити пам’ять під RAM-диск
    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
                               (ramdisk_size + 0xFFF) / 0x1000, (EFI_PHYSICAL_ADDRESS *)&ramdisk_addr);
    if (EFI_ERROR(status))
    {
        Print(L"AllocatePages failed: %r\n", status);
        return status;
    }

    // Зчитати в пам’ять
    status = uefi_call_wrapper(RamdiskFile->Read, 3, RamdiskFile, &ramdisk_size, ramdisk_addr);
    if (EFI_ERROR(status))
    {
        Print(L"Read failed: %r\n", status);
        return status;
    }

    // Закрити файл
    uefi_call_wrapper(RamdiskFile->Close, 1, RamdiskFile);

    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    uefi_call_wrapper(RootFS->Close, 1, RootFS);

    // === [4] Виділення місця для framebuffer_info ===
    framebuffer_info_t *fb_info = (framebuffer_info_t *)((uint8_t *)kernel_addr + kernel_size + 0x1000);

    // === [4] Виділення місця для ram_info ===
    ram_info_t *ram_info = (ram_info_t *)((uint8_t *)fb_info + sizeof(framebuffer_info_t));
    ramdisk_info_t *ramdisk_info = (ramdisk_info_t *)((uint8_t *)ram_info + sizeof(ram_info_t));

    fb_info->base = (void *)gop->Mode->FrameBufferBase;
    fb_info->width = gop->Mode->Info->HorizontalResolution;
    fb_info->height = gop->Mode->Info->VerticalResolution;
    fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    fb_info->bpp = 32;

    ramdisk_info->ramdisk_base = ramdisk_addr;
    ramdisk_info->ramdisk_size = ramdisk_size;

    // === [5] Find largest EfiConventionalMemory region for heap ===
    EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
    UINTN mem_map_size = 0, map_key, desc_size;
    UINT32 desc_version;

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL)
    {
        Print(L"[Debug] GetMemoryMap failed: %r\n", status);
        return status;
    }

    mem_map_size += desc_size * 10;
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mem_map_size, (void **)&mem_map);
    if (EFI_ERROR(status))
    {
        Print(L"[Debug] AllocatePool failed: %r\n", status);
        return status;
    }

    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status))
    {
        Print(L"[Debug] GetMemoryMap failed: %r\n", status);
        return status;
    }

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

    ram_info->heap_start = best->PhysicalStart;
    ram_info->heap_size = best->NumberOfPages * EFI_PAGE_SIZE;

    Print(L"[Debug] heap_start=%lx, heap_size=%lx\n", ram_info->heap_start, ram_info->heap_size);

    BootInfo boot_info = {fb_info, ram_info, ramdisk_info};

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
    void (*kernel_entry)(BootInfo *) = (void *)kernel_addr;
    kernel_entry(&boot_info);

    return EFI_SUCCESS;
}
