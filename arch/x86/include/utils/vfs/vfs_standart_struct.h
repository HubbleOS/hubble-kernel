#pragma once
#include <stdint.h>

typedef struct
{
	uint32_t cluster;
	char *name;
	bool is_dir;
} Entry;
typedef struct
{
	Entry *entries;
	int count;
} Directory;

typedef enum
{
	OPEN_READ = 'r',
	OPEN_WRITE = 'w',
	OPEN_BYTE = 'b',
	OPEN_APPEND = 'a',
	OPEN_CREATE = '+'
} OpenFlags;

typedef enum
{
	FS_NONE,
	FS_FAT32,
	FS_EXT2,
	// інші
} FileSystemType;
typedef struct
{
	void *device;
	int (*read)(void *device, uint32_t lba, void *buffer);
	int (*write)(void *device, uint32_t lba, const void *buffer);
} VFS_Device;

#define ERR_PTR(x) ((void *)(intptr_t)(x))
#define PTR_ERR(p) ((int)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)

// Open modes
#define VFS_O_RDONLY 0x01 // RD
#define VFS_O_WRONLY 0x02 // WR
#define VFS_O_RDWR 0x03	  // RD | WR

#define VFS_O_CREAT 0x10  // Create file if not exist
#define VFS_O_EXCL 0x20	  // Error if file exist
#define VFS_O_TRUNC 0x40  // Truncate file
#define VFS_O_APPEND 0x80 // Append to file
// Standart file modes
#define MODE_FILE 0x1000    // File
#define MODE_DIR 0x2000	    // Directory
#define MODE_CHAR 0x3000    // Character device
#define MODE_BLOCK 0x4000   // Block device
#define MODE_PIPE 0x5000    // Pipe
#define MODE_SYMLINK 0x6000 // Symbolic link

// Access modes
#define MODE_READ 0x0004  // Read access
#define MODE_WRITE 0x0002 // Write access
#define MODE_EXEC 0x0001  // Execute access

// Seek modes
#define SEEK_SET 0 // Set position
#define SEEK_CUR 1 // Current position
#define SEEK_END 2 // End position

// extern void *fb;
