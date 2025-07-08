#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"
#include "fat.h"

// #include <stdint.h>
#include <stdio.h>

extern void os_main(framebuffer_info_t *fb);
extern void libc_init(void);

void kernel_main(BootInfo *bi)
{

    heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
    framebuffer_info_t *fb = bi->framebuffer;

    fat32_init(bi->disk_info->ramdisk_base);

    init_font(fb);
    libc_init();

    char buf[4096];
    size_t sz;

    debug_fat32(fb);
    if (fat32_read_file("TEXT.TXT", buf, &sz) == 0)
    {
        printf("FILE FOUND\n");
        printf("%s\n", buf);
        if (fat32_delete_file("TEXT.TXT") == 0)
        {
            printf("FILE DELETED\n");
            if (fat32_read_file("TEXT.TXT", buf, &sz) == 0)
            {
                printf("FILE FOUND\n");
                printf("%s\n", buf);
            }
        }
    }
    else
    {
        printf("FILE NOT FOUND\n");
    }
    printf("END\n");

    while (1)
        ;
}
