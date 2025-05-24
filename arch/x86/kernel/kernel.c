#include "utils/framebuffer.h"
#include "utils/font.h"
#include "cli.h"
#include "heap.h"

#include <stdint.h>

#include <stdio.h>

extern framebuffer_info_t *g_fb;
extern int cursor_x;
extern int cursor_y;

void os_main(framebuffer_info_t *fb);

void kernel_main(framebuffer_info_t *fb)
{
    heap_init(fb->heap_start, fb->heap_size);
    init_font(fb);
    stdio_init();

    os_main(fb);

    while (1)
        ;
}
