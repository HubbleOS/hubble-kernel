#include "utils/framebuffer.h"
#include "utils/font.h"
#include "cli.h"
#include "heap.h"

void kernel_main(framebuffer_info_t *fb)
{

    heap_init(fb->heap_start, fb->heap_size);
    print_text(fb, "Hello World!", 100, 100, 0x00ffffff);
    // uint32_t *pixels = (uint32_t *)fb->base;
    // for (int i = 0; i < (fb->pitch / 4) * fb->height; ++i)
    // {
    //     pixels[i] = 0x000000;
    // }

    cli(fb);
    while (1)
        ;
}
