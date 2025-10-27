#pragma once

#include <_cheader.h>
#include <bootinfo/framebuffer.h>
#include <stdint.h>

_Begin_C_Header;

void play_bwvid(framebuffer_info_t *bi, const char *path, uint32_t pitch, uint32_t bpp, int x, int y);

_End_C_Header;
