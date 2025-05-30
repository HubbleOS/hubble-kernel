#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include <stdint.h>
typedef struct
{
    void *base;
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    unsigned int bpp;
} framebuffer_info_t;
typedef struct
{
    void *ramdisk_base;
    uint64_t ramdisk_size;
} __attribute__((packed)) ramdisk_info_t;
typedef struct
{
    uint64_t heap_start;
    uint64_t heap_size;
} ram_info_t;
typedef struct
{
    framebuffer_info_t *framebuffer;
    ram_info_t *memory_map;
    ramdisk_info_t *disk_info;
} BootInfo;

#endif
