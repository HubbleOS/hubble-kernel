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

#define VFS_O_RDONLY 0x01
#define VFS_O_WRONLY 0x02
#define VFS_O_RDWR 0x03 // RD | WR

#define VFS_O_CREAT 0x10
#define VFS_O_EXCL 0x20
#define VFS_O_TRUNC 0x40
#define VFS_O_APPEND 0x80

#define MODE_FILE 0x1000
#define MODE_DIR 0x2000
#define MODE_CHAR 0x3000
#define MODE_BLOCK 0x4000
#define MODE_PIPE 0x5000
#define MODE_SYMLINK 0x6000

// Маски доступу
#define MODE_READ 0x0004
#define MODE_WRITE 0x0002
#define MODE_EXEC 0x0001