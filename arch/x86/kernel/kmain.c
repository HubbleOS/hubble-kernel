#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"

#include <stdint.h>

extern framebuffer_info_t *g_fb;
extern int cursor_x;
extern int cursor_y;

void os_main(framebuffer_info_t *fb);

extern void libc_init(void);

void kernel_main(framebuffer_info_t *fb)
{
    heap_init(fb->heap_start, fb->heap_size);
    init_font(fb);
    libc_init();

    os_main(fb);
    return;
    while (1)
        ;
}
