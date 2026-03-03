#pragma once
#include <stdint.h>
#include <stddef.h>

#include <bootinfo/framebuffer.h>
#include "utils/color.h"

extern framebuffer_info_t *g_fb;
extern uint32_t *framebuffer_back;
extern int fb_width;
extern int fb_height;

void screen_init(framebuffer_info_t *fb);
void screen_present(void);
void screen_present_rect(int x, int y, int w, int h);
