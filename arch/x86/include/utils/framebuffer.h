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
    uint64_t heap_start;
    uint64_t heap_size;
} framebuffer_info_t;

#endif
