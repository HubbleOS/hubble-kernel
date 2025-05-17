#include <efi.h>
#include <efilib.h>
#include "utils/framebuffer.h"

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    InitializeLib(image, systab);
    Print(L"[1] Bootloader started\n");

    // === [2] Ініціалізація GOP ===
    Print(L"[2] Locating GOP\n");
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status;

    status = uefi_call_wrapper(systab->BootServices->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"[2] GOP not found: %r\n", status);
        return status;
    }

    // === [3] Ініціалізація framebuffer_info_t ===
    Print(L"[3] Setting framebuffer info\n");
    framebuffer_info_t *fb_info = (framebuffer_info_t *)0x9000;
    fb_info->base = (void *)gop->Mode->FrameBufferBase;
    fb_info->width = gop->Mode->Info->HorizontalResolution;
    fb_info->height = gop->Mode->Info->VerticalResolution;
    fb_info->pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    fb_info->bpp = 32;

    // === [4] Відкриття файлової системи ===
    Print(L"[4] Opening file system\n");
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_FILE_IO_INTERFACE *FileIO;
    EFI_FILE_HANDLE RootFS, KernelFile;

    status = uefi_call_wrapper(systab->BootServices->LocateProtocol, 3, &fs_guid, NULL, (void **)&FileIO);
    if (EFI_ERROR(status))
    {
        Print(L"[4] File IO not found: %r\n", status);
        return status;
    }

    status = uefi_call_wrapper(FileIO->OpenVolume, 2, FileIO, &RootFS);
    if (EFI_ERROR(status))
    {
        Print(L"[4] OpenVolume failed: %r\n", status);
        return status;
    }

    // === [5] Відкриття kernel.bin ===
    Print(L"[5] Opening kernel.bin\n");
    status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &KernelFile, L"kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        Print(L"[5] kernel.bin not found: %r\n", status);
        return status;
    }

    // === [6] Читання ядра ===
    Print(L"[6] Reading kernel.bin into memory\n");
    void *kernel_addr = (void *)0x100000;
    UINTN size = 1024 * 1024;
    status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &size, kernel_addr);
    if (EFI_ERROR(status))
    {
        Print(L"[6] Kernel read error: %r\n", status);
        return status;
    }

    // === [7] Закриття файлової системи ===
    Print(L"[7] Closing file system\n");
    status = uefi_call_wrapper(RootFS->Close, 1, RootFS);
    if (EFI_ERROR(status))
    {
        Print(L"[7] Close failed: %r\n", status);
        return status;
    }

    // === [8] Закриття kernel.bin ===
    Print(L"[8] Closing kernel.bin\n");
    status = uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    if (EFI_ERROR(status))
    {
        Print(L"[8] Close failed: %r\n", status);
        return status;
    }

    // === [9] Перехід до ядра ===
    Print(L"[9] Jumping to kernel at 0x100000\n");
    void (*kernel_main)(framebuffer_info_t *) = (void *)0x100000;
    kernel_main(fb_info);

    // === [10] Кінець (не повинен досягатися) ===
    Print(L"[10] Kernel returned!\n");
    return EFI_SUCCESS;
}
