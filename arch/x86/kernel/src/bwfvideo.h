/**
 * @file bwfvideo.h
 * @brief Black-and-white frame video player header
 */

#pragma once

#include <_cheader.h>
#include <hubble/fb.h>
#include <stdint.h>

_Begin_C_Header;

void play_bwvid(framebuffer_info_t *bi, const char *path, int x, int y);

_End_C_Header;
