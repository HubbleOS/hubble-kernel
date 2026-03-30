#pragma once

// Basic types
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef unsigned long long UINTN;
typedef long long INTN;
typedef unsigned short CHAR16;
typedef void *EFI_HANDLE;
typedef void *EFI_EVENT;
typedef UINT64 EFI_STATUS;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

#define EFIAPI __attribute__((ms_abi))

// Statuses
#define EFI_SUCCESS 0ULL
#define EFI_ERROR_BIT (1ULL << 63)
#define EFI_ERROR(s) ((s) & EFI_ERROR_BIT)
#define EFI_BUFFER_TOO_SMALL (EFI_ERROR_BIT | 5)
#define EFI_NOT_FOUND (EFI_ERROR_BIT | 14)
#define EFI_OUT_OF_RESOURCES (EFI_ERROR_BIT | 9)
#define EFI_INVALID_PARAMETER (EFI_ERROR_BIT | 2)
#define EFI_UNSUPPORTED (EFI_ERROR_BIT | 3)

// GUID
typedef struct
{
	UINT32 Data1;
	UINT16 Data2;
	UINT16 Data3;
	UINT8 Data4[8];
} EFI_GUID;

static inline int efi_guid_equal(EFI_GUID *a, EFI_GUID *b)
{
	return a->Data1 == b->Data1 &&
	       a->Data2 == b->Data2 &&
	       a->Data3 == b->Data3 &&
	       ((UINT64 *)a->Data4)[0] == ((UINT64 *)b->Data4)[0];
}

// Memory
typedef enum
{
	EfiReservedMemoryType,
	EfiLoaderCode,
	EfiLoaderData,
	EfiBootServicesCode,
	EfiBootServicesData,
	EfiRuntimeServicesCode,
	EfiRuntimeServicesData,
	EfiConventionalMemory,
	EfiUnusableMemory,
	EfiACPIReclaimMemory,
	EfiACPIMemoryNVS,
	EfiMemoryMappedIO,
	EfiMemoryMappedIOPortSpace,
	EfiPalCode,
	EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef struct
{
	UINT32 Type;
	UINT32 Pad;
	EFI_PHYSICAL_ADDRESS PhysicalStart;
	EFI_VIRTUAL_ADDRESS VirtualStart;
	UINT64 NumberOfPages;
	UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

#define EFI_PAGE_SIZE 0x1000

typedef enum
{
	AllocateAnyPages,
	AllocateMaxAddress,
	AllocateAddress,
} EFI_ALLOCATE_TYPE;

// Boot Services function types
typedef EFI_STATUS(EFIAPI *EFI_RAISE_TPL)(UINTN new_tpl);
typedef EFI_STATUS(EFIAPI *EFI_RESTORE_TPL)(UINTN old_tpl);

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_PAGES)(
    EFI_ALLOCATE_TYPE type,
    EFI_MEMORY_TYPE mem_type,
    UINTN pages,
    EFI_PHYSICAL_ADDRESS *memory);

typedef EFI_STATUS(EFIAPI *EFI_FREE_PAGES)(
    EFI_PHYSICAL_ADDRESS memory,
    UINTN pages);

typedef EFI_STATUS(EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN *map_size,
    EFI_MEMORY_DESCRIPTOR *map,
    UINTN *map_key,
    UINTN *desc_size,
    UINT32 *desc_version);

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_POOL)(
    EFI_MEMORY_TYPE pool_type,
    UINTN size,
    void **buffer);

typedef EFI_STATUS(EFIAPI *EFI_FREE_POOL)(void *buffer);

typedef EFI_STATUS(EFIAPI *EFI_CREATE_EVENT)(void);
typedef EFI_STATUS(EFIAPI *EFI_SET_TIMER)(void);
typedef EFI_STATUS(EFIAPI *EFI_WAIT_FOR_EVENT)(void);
typedef EFI_STATUS(EFIAPI *EFI_SIGNAL_EVENT)(void);
typedef EFI_STATUS(EFIAPI *EFI_CLOSE_EVENT)(void);
typedef EFI_STATUS(EFIAPI *EFI_CHECK_EVENT)(void);

typedef EFI_STATUS(EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE)(void);
typedef EFI_STATUS(EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE)(void);
typedef EFI_STATUS(EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE)(void);

typedef EFI_STATUS(EFIAPI *EFI_HANDLE_PROTOCOL)(
    EFI_HANDLE handle,
    EFI_GUID *protocol,
    void **interface);

typedef EFI_STATUS(EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY)(void);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_HANDLE)(void);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_DEVICE_PATH)(void);

typedef EFI_STATUS(EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE)(void);

typedef EFI_STATUS(EFIAPI *EFI_IMAGE_LOAD)(void);
typedef EFI_STATUS(EFIAPI *EFI_IMAGE_START)(void);
typedef EFI_STATUS(EFIAPI *EFI_EXIT)(void);
typedef EFI_STATUS(EFIAPI *EFI_IMAGE_UNLOAD)(void);

typedef EFI_STATUS(EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE image,
    UINTN map_key);

typedef EFI_STATUS(EFIAPI *EFI_GET_NEXT_MONOTONIC_COUNT)(void);
typedef EFI_STATUS(EFIAPI *EFI_STALL)(void);
typedef EFI_STATUS(EFIAPI *EFI_SET_WATCHDOG_TIMER)(void);

typedef EFI_STATUS(EFIAPI *EFI_CONNECT_CONTROLLER)(void);
typedef EFI_STATUS(EFIAPI *EFI_DISCONNECT_CONTROLLER)(void);

typedef EFI_STATUS(EFIAPI *EFI_OPEN_PROTOCOL)(void);
typedef EFI_STATUS(EFIAPI *EFI_CLOSE_PROTOCOL)(void);
typedef EFI_STATUS(EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION)(void);

typedef EFI_STATUS(EFIAPI *EFI_PROTOCOLS_PER_HANDLE)(void);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    UINT32 search_type,
    EFI_GUID *protocol,
    void *search_key,
    UINTN *count,
    EFI_HANDLE **buffer);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_PROTOCOL)(
    EFI_GUID *protocol,
    void *registration,
    void **interface);

typedef EFI_STATUS(EFIAPI *EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)(void);
typedef EFI_STATUS(EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(void);

typedef EFI_STATUS(EFIAPI *EFI_CALCULATE_CRC32)(void);
typedef EFI_STATUS(EFIAPI *EFI_COPY_MEM)(void);
typedef EFI_STATUS(EFIAPI *EFI_SET_MEM)(void);
typedef EFI_STATUS(EFIAPI *EFI_CREATE_EVENT_EX)(void);

// EFI_TABLE_HEADER
typedef struct
{
	UINT64 Signature;
	UINT32 Revision;
	UINT32 HeaderSize;
	UINT32 CRC32;
	UINT32 Reserved;
} EFI_TABLE_HEADER;

// Boot Services — все поля в правильном порядке (UEFI spec 2.x)
typedef struct
{
	EFI_TABLE_HEADER Hdr;

	// Task Priority
	EFI_RAISE_TPL RaiseTPL;
	EFI_RESTORE_TPL RestoreTPL;

	// Memory
	EFI_ALLOCATE_PAGES AllocatePages;
	EFI_FREE_PAGES FreePages;
	EFI_GET_MEMORY_MAP GetMemoryMap;
	EFI_ALLOCATE_POOL AllocatePool;
	EFI_FREE_POOL FreePool;

	// Events
	EFI_CREATE_EVENT CreateEvent;
	EFI_SET_TIMER SetTimer;
	EFI_WAIT_FOR_EVENT WaitForEvent;
	EFI_SIGNAL_EVENT SignalEvent;
	EFI_CLOSE_EVENT CloseEvent;
	EFI_CHECK_EVENT CheckEvent;

	// Protocol handlers
	EFI_INSTALL_PROTOCOL_INTERFACE InstallProtocolInterface;
	EFI_REINSTALL_PROTOCOL_INTERFACE ReinstallProtocolInterface;
	EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface;
	EFI_HANDLE_PROTOCOL HandleProtocol;
	void *Reserved;
	EFI_REGISTER_PROTOCOL_NOTIFY RegisterProtocolNotify;
	EFI_LOCATE_HANDLE LocateHandle;
	EFI_LOCATE_DEVICE_PATH LocateDevicePath;
	EFI_INSTALL_CONFIGURATION_TABLE InstallConfigurationTable;

	// Image
	EFI_IMAGE_LOAD LoadImage;
	EFI_IMAGE_START StartImage;
	EFI_EXIT Exit;
	EFI_IMAGE_UNLOAD UnloadImage;
	EFI_EXIT_BOOT_SERVICES ExitBootServices;

	// Misc
	EFI_GET_NEXT_MONOTONIC_COUNT GetNextMonotonicCount;
	EFI_STALL Stall;
	EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;

	// Driver support
	EFI_CONNECT_CONTROLLER ConnectController;
	EFI_DISCONNECT_CONTROLLER DisconnectController;

	// Open/close protocol
	EFI_OPEN_PROTOCOL OpenProtocol;
	EFI_CLOSE_PROTOCOL CloseProtocol;
	EFI_OPEN_PROTOCOL_INFORMATION OpenProtocolInformation;

	// Library
	EFI_PROTOCOLS_PER_HANDLE ProtocolsPerHandle;
	EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
	EFI_LOCATE_PROTOCOL LocateProtocol;
	EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES InstallMultipleProtocolInterfaces;
	EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES UninstallMultipleProtocolInterfaces;

	// CRC
	EFI_CALCULATE_CRC32 CalculateCrc32;

	// Memory utils
	EFI_COPY_MEM CopyMem;
	EFI_SET_MEM SetMem;
	EFI_CREATE_EVENT_EX CreateEventEx;
} EFI_BOOT_SERVICES;

// ConOut
typedef EFI_STATUS(EFIAPI *EFI_TEXT_STRING)(
    void *this,
    CHAR16 *string);

typedef struct
{
	void *Reset;
	EFI_TEXT_STRING OutputString;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// Configuration Table (для RSDP)
typedef struct
{
	EFI_GUID VendorGuid;
	void *VendorTable;
} EFI_CONFIGURATION_TABLE;

// System Table
typedef struct
{
	EFI_TABLE_HEADER Hdr;
	CHAR16 *FirmwareVendor;
	UINT32 FirmwareRevision;
	EFI_HANDLE ConsoleInHandle;
	void *ConIn;
	EFI_HANDLE ConsoleOutHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
	EFI_HANDLE StandardErrorHandle;
	void *StdErr;
	void *RuntimeServices;
	EFI_BOOT_SERVICES *BootServices;
	UINTN NumberOfTableEntries;
	EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

// File Protocol
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef EFI_FILE_PROTOCOL *EFI_FILE_HANDLE;

typedef EFI_STATUS(EFIAPI *EFI_FILE_OPEN)(
    EFI_FILE_PROTOCOL *this,
    EFI_FILE_PROTOCOL **new_handle,
    CHAR16 *filename,
    UINT64 open_mode,
    UINT64 attributes);

typedef EFI_STATUS(EFIAPI *EFI_FILE_CLOSE)(
    EFI_FILE_PROTOCOL *this);

typedef EFI_STATUS(EFIAPI *EFI_FILE_READ)(
    EFI_FILE_PROTOCOL *this,
    UINTN *buffer_size,
    void *buffer);

typedef EFI_STATUS(EFIAPI *EFI_FILE_GET_INFO)(
    EFI_FILE_PROTOCOL *this,
    EFI_GUID *info_type,
    UINTN *buffer_size,
    void *buffer);

struct _EFI_FILE_PROTOCOL
{
	UINT64 Revision;
	EFI_FILE_OPEN Open;
	EFI_FILE_CLOSE Close;
	void *Delete;
	EFI_FILE_READ Read;
	void *Write;
	void *GetPosition;
	void *SetPosition;
	EFI_FILE_GET_INFO GetInfo;
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

typedef struct
{
	UINT64 Size;
	UINT64 FileSize;
	UINT64 PhysicalSize;
	UINT8 CreateTime[16];
	UINT8 LastAccessTime[16];
	UINT8 ModificationTime[16];
	UINT64 Attribute;
	CHAR16 FileName[1];
} EFI_FILE_INFO;

#define EFI_FILE_INFO_ID \
	{0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

// Simple File System Protocol
typedef EFI_STATUS(EFIAPI *EFI_SIMPLE_FILE_SYSTEM_OPEN_VOLUME)(
    void *this,
    EFI_FILE_PROTOCOL **root);

typedef struct
{
	UINT64 Revision;
	EFI_SIMPLE_FILE_SYSTEM_OPEN_VOLUME OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
	{0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

// GOP
typedef struct
{
	UINT32 RedMask;
	UINT32 GreenMask;
	UINT32 BlueMask;
	UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum
{
	PixelRedGreenBlueReserved8BitPerColor,
	PixelBlueGreenRedReserved8BitPerColor,
	PixelBitMask,
	PixelBltOnly,
	PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct
{
	UINT32 Version;
	UINT32 HorizontalResolution;
	UINT32 VerticalResolution;
	EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
	EFI_PIXEL_BITMASK PixelInformation;
	UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct
{
	UINT32 MaxMode;
	UINT32 Mode;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
	UINTN SizeOfInfo;
	EFI_PHYSICAL_ADDRESS FrameBufferBase;
	UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct
{
	void *QueryMode;
	void *SetMode;
	void *Blt;
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
	{0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}}

// LocateHandleBuffer search types

#define ByProtocol 2
