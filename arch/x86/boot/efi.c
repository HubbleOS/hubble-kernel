/**
 * @file efi.c
 * @brief UEFI application entry — locates kernel ELF, sets up memory map,
 * transitions to boot_main
 */

#include "efi.h"
#include "loader_context.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "higher_half.h"

EFI_HANDLE g_image;
EFI_SYSTEM_TABLE *g_systab;

extern void boot_main(loader_context_t *ctx);

static EFI_GUID Acpi20Guid = {0x8868e871,
                              0xe4f1,
                              0x11d3,
                              {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
static EFI_GUID Acpi10Guid = {0xeb9d2d30,
                              0x2d88,
                              0x11d3,
                              {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};

/** @brief Find the RSDP pointer in the EFI configuration table */
static void *find_rsdp(EFI_SYSTEM_TABLE *SystemTable) {
  for (UINTN i = 0; i < g_systab->NumberOfTableEntries; i++) {
    EFI_CONFIGURATION_TABLE *tbl = &g_systab->ConfigurationTable[i];

    if (memcmp(&tbl->VendorGuid, &Acpi20Guid, sizeof(EFI_GUID)) == 0) {
      char *sig = (char *)tbl->VendorTable;
      if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D')
        return tbl->VendorTable;
    }

    if (memcmp(&tbl->VendorGuid, &Acpi10Guid, sizeof(EFI_GUID)) == 0) {
      char *sig = (char *)tbl->VendorTable;
      if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D')
        return tbl->VendorTable;
    }
  }

  return NULL;
}

/** @brief Initialise framebuffer via EFI Graphics Output Protocol */
EFI_STATUS init_framebuffer(EFI_SYSTEM_TABLE *systab, fb_info_t *fb_info) {
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

/** @brief Open a file on any EFI simple file system volume */
EFI_STATUS open_file(EFI_SYSTEM_TABLE *systab, const CHAR16 *path,
                     EFI_FILE_HANDLE *file) {
  EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
  EFI_HANDLE *handle_buffer = NULL;
  UINTN handle_count = 0;
  EFI_STATUS status;

  *file = NULL;

  status = systab->BootServices->LocateHandleBuffer(
      ByProtocol, &fs_guid, NULL, &handle_count, &handle_buffer);
  if (EFI_ERROR(status))
    return status;

  for (UINTN i = 0; i < handle_count; i++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_HANDLE root;

    status = systab->BootServices->HandleProtocol(handle_buffer[i], &fs_guid,
                                                  (void **)&fs);
    if (EFI_ERROR(status))
      continue;

    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status))
      continue;

    status = root->Open(root, file, path, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(status)) {
      root->Close(root);
      break;
    }

    root->Close(root);
  }

  if (handle_buffer)
    systab->BootServices->FreePool(handle_buffer);

  return (*file != NULL) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/** @brief Get the size of an open EFI file */
EFI_STATUS get_file_size(EFI_FILE_HANDLE file, EFI_SYSTEM_TABLE *systab,
                         UINTN *size) {
  EFI_STATUS status;
  EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
  EFI_FILE_INFO *file_info = NULL;
  UINTN info_size = 0;

  status = file->GetInfo(file, &FileInfoGuid, &info_size, NULL);
  if (status != EFI_BUFFER_TOO_SMALL)
    return status;

  status = systab->BootServices->AllocatePool(EfiLoaderData, info_size,
                                              (void **)&file_info);
  if (EFI_ERROR(status))
    return status;

  status = file->GetInfo(file, &FileInfoGuid, &info_size, file_info);
  if (!EFI_ERROR(status))
    *size = file_info->FileSize;

  systab->BootServices->FreePool(file_info);
  return status;
}

/** @brief Convert EFI memory type to internal mem_type_t */
static mem_type_t efi_to_mem_type(UINT32 efi_type) {
  switch (efi_type) {
  case EfiConventionalMemory:
    return MEM_CONVENTIONAL;
  case EfiLoaderCode:
    return MEM_LOADER_CODE;
  case EfiLoaderData:
    return MEM_LOADER_DATA;
  case EfiACPIReclaimMemory:
  case EfiACPIMemoryNVS:
    return MEM_ACPI;
  case EfiMemoryMappedIO:
  case EfiMemoryMappedIOPortSpace:
    return MEM_MMIO;
  case EfiReservedMemoryType:
    return MEM_RESERVED;
  default:
    return MEM_OTHER;
  }
}

/** @brief UEFI application entry point */
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab) {
  g_image = image;
  g_systab = systab;
  EFI_STATUS status;

  /* -- 1. Open kernel.elf and get its size -- */
  EFI_FILE_HANDLE kfile;
  status = open_file(systab, L"\\kernel.elf", &kfile);
  if (EFI_ERROR(status))
    return status;

  UINTN kernel_size;
  status = get_file_size(kfile, systab, &kernel_size);
  if (EFI_ERROR(status))
    return status;

  /* -- 2. Reserve physical memory for the kernel image at KERNEL_PHYS_BASE --
   */
  EFI_PHYSICAL_ADDRESS kernel_phys = KERNEL_PHYS_BASE;
  UINTN kernel_pages = (kernel_size + 0xFFF) / 0x1000;
  status = systab->BootServices->AllocatePages(AllocateAddress, EfiLoaderData,
                                               kernel_pages, &kernel_phys);
  if (EFI_ERROR(status))
    return status;

  /* -- 3. Read the ELF into a page-aligned buffer -- */
  EFI_PHYSICAL_ADDRESS elf_phys;
  UINTN elf_pages = (kernel_size + 0xFFF) / 0x1000;
  status = systab->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                               elf_pages, &elf_phys);
  if (EFI_ERROR(status))
    return status;
  void *elf_buf = (void *)elf_phys;

  UINTN total = kernel_size;
  UINTN offset = 0;
  while (offset < total) {
    UINTN chunk = total - offset;
    status = kfile->Read(kfile, &chunk, (uint8_t *)elf_buf + offset);
    if (EFI_ERROR(status) || chunk == 0)
      break;
    offset += chunk;
  }
  kernel_size = offset;

  kfile->Close(kfile);

  /* -- 4. Get RSDP -- */
  void *rsdp = find_rsdp(systab);
  if (!rsdp)
    return EFI_NOT_FOUND;

  /* -- 5. Initialise framebuffer -- */
  fb_info_t fb;
  status = init_framebuffer(systab, &fb);
  if (EFI_ERROR(status))
    return status;

  /* -- 6. Memory map (before ExitBootServices) -- */
  EFI_MEMORY_DESCRIPTOR *efi_map = NULL;
  UINTN map_size = 0, map_key, desc_size;
  UINT32 desc_ver;

  systab->BootServices->GetMemoryMap(&map_size, NULL, &map_key, &desc_size,
                                     &desc_ver);
  map_size += desc_size * 16;

  systab->BootServices->AllocatePool(EfiLoaderData, map_size,
                                     (void **)&efi_map);
  status = systab->BootServices->GetMemoryMap(&map_size, efi_map, &map_key,
                                              &desc_size, &desc_ver);
  if (EFI_ERROR(status))
    return status;

  UINTN efi_count = map_size / desc_size;

  /* -- 7. Find the largest free region -- */
  EFI_MEMORY_DESCRIPTOR *largest = NULL;
  UINT64 largest_size = 0;
  for (UINTN i = 0; i < efi_count; i++) {
    EFI_MEMORY_DESCRIPTOR *d =
        (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)efi_map + i * desc_size);
    if (d->Type == EfiConventionalMemory) {
      UINT64 sz = d->NumberOfPages * EFI_PAGE_SIZE;
      if (sz > largest_size) {
        largest_size = sz;
        largest = d;
      }
    }
  }
  if (!largest || largest_size < 64 * 1024 * 1024)
    return EFI_OUT_OF_RESOURCES;

  /* -- 8. Build our memory map -- */
  mem_descriptor_t *our_map = NULL;
  systab->BootServices->AllocatePool(
      EfiLoaderData, efi_count * sizeof(mem_descriptor_t), (void **)&our_map);
  for (UINTN i = 0; i < efi_count; i++) {
    EFI_MEMORY_DESCRIPTOR *d =
        (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)efi_map + i * desc_size);
    our_map[i].phys_start = d->PhysicalStart;
    our_map[i].num_pages = d->NumberOfPages;
    our_map[i].type = efi_to_mem_type(d->Type);
  }

  /* -- 9. Fill LoaderContext -- */
  loader_context_t *ctx = NULL;
  systab->BootServices->AllocatePool(EfiLoaderData, sizeof(loader_context_t),
                                     (void **)&ctx);

  ctx->elf_buf = elf_buf;
  ctx->elf_size = kernel_size;
  ctx->mem_map = our_map;
  ctx->mem_map_count = efi_count;
  ctx->framebuffer = fb;
  ctx->rsdp = rsdp;
  ctx->free_phys_base = largest->PhysicalStart;
  ctx->free_phys_size = largest_size;

  /* -- 10. ExitBootServices -- */
  map_size = 0;
  systab->BootServices->GetMemoryMap(&map_size, NULL, &map_key, &desc_size,
                                     &desc_ver);
  map_size += desc_size * 4;
  EFI_MEMORY_DESCRIPTOR *temp = NULL;
  systab->BootServices->AllocatePool(EfiLoaderData, map_size, (void **)&temp);
  systab->BootServices->GetMemoryMap(&map_size, temp, &map_key, &desc_size,
                                     &desc_ver);

  status = systab->BootServices->ExitBootServices(image, map_key);
  if (EFI_ERROR(status))
    return status;

  /* -- POINT OF NO RETURN -- */

  boot_main(ctx);

  while (1)
    __asm__("hlt");
  return EFI_SUCCESS;
}
