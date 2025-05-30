#include "utils/framebuffer.h"
#include "utils/font.h"
#include "cli.h"
#include "heap.h"
#include "fat.h"

void kernel_main(BootInfo *bi)
{

    heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
    framebuffer_info_t *fb = bi->framebuffer;

    fat32_init(bi->disk_info->ramdisk_base);

    char buf[4096];
    size_t sz;

    // debug_fat32(fb);
    if (fat32_read_file("TEXT.TXT", buf, &sz, fb) == 0)
    {
        print_text(fb, buf, 50, 50, 0xFFFFFF);
        // файл прочитано!
        print_text(fb, "fuck you!", 100, 100, 0xFFFFFF);
    }
    else
    {
    }
    if (fat32_write_file("TEST.TXT", "Hello world!", 12) == 0)
    {
        print_text(fb, "file written!", 100, 200, 0xFFFFFF);
    }
    if (fat32_read_file("TEST.TXT", buf, &sz, fb) == 0)
    {
        print_text(fb, buf, 50, 300, 0xFFFFFF);
    }

    // uint32_t *pixels = (uint32_t *)fb->base;
    // for (int i = 0; i < (fb->pitch / 4) * fb->height; ++i)
    // {
    //     pixels[i] = 0x000000;
    // }

    // cli(fb);
    while (1)
        ;
}
