#pragma once
#include <stdint.h>
#include <stddef.h>
#include <gui/utils/color/color.h>
#ifndef FBSIM
#include <bootinfo/framebuffer.h>
#else

#include "main.h"
#endif
extern framebuffer_info_t *g_fb;
extern uint32_t *framebuffer_back;
extern int fb_width;
extern int fb_height;

void screen_init(framebuffer_info_t *fb);
void screen_present_rect(int x, int y, int w, int h);
