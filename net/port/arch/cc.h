/*
 * lwIP platform definitions for the kernel (see lwip/arch.h).
 */
#pragma once

#include <hubble/printk.h>
#include <stddef.h>
#include <stdint.h>

/* Freestanding: no <inttypes.h> or <unistd.h> (the kernel libc in
 * include/hubble has string.h, ctype.h, stdio.h and stdlib.h). */
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_UNISTD_H 1
#define X8_F "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "zu"

typedef long ssize_t;
#define SSIZE_MAX __LONG_MAX__

#define BYTE_ORDER LITTLE_ENDIAN

#define LWIP_PLATFORM_DIAG(x)                                                  \
  do {                                                                         \
    printk x;                                                                  \
  } while (0)

void net_lwip_assert(const char *msg, const char *file, int line);
#define LWIP_PLATFORM_ASSERT(x) net_lwip_assert(x, __FILE__, __LINE__)

uint32_t net_rand(void);
#define LWIP_RAND() net_rand()
