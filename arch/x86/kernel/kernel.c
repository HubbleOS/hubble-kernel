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
    if (fat32_read_file("README.TXT", buf, &sz, fb) == 0)
    {
        print_text(fb, buf, 50, 50, 0xFFFFFF);
        // файл прочитано!
        print_text(fb, "fuck you!", 100, 100, 0xFFFFFF);
    }
    if (fat32_read_file("/texts/README.TXT", buf, &sz, fb) == 0)
    {
        print_text(fb, "FILE FOUND", 50, 100, 0xFFFFFF);
        print_text(fb, buf, 50, 150, 0xFFFFFF);
    }
    else
    {
        print_text(fb, "FILE NOT FOUND", 50, 150, 0xFFFFFF);
    }
    print_text(fb, "END", 50, 200, 0xFFFFFF);

    // uint32_t *pixels = (uint32_t *)fb->base;
    // for (int i = 0; i < (fb->pitch / 4) * fb->height; ++i)
    // {
    //     pixels[i] = 0x000000;
    // }

    // cli(fb);
    while (1)
        ;
}
