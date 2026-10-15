/*
 * peload.h - what the PE loader (peload.c) and the Windows API it
 * provides (win32.c) share.
 */
#pragma once

#include <stdint.h>

/* Every function the .exe calls is ms_abi: arguments in RCX RDX R8 R9,
 * 32 bytes of shadow space, other callee-saved registers than the SysV
 * ABI. gcc handles all of it from the attribute. */
#define WINAPI __attribute__((ms_abi))

struct func {
  const char *name;
  void *addr;
};

/* -- win32.c -------------------------------------------------------------- */

/** Our implementation of dll!name, or NULL. */
void *find_import(const char *dll, const char *name);

/** Set up what Windows gives a process before its entry point runs: the
 * TEB/PEB behind %gs, and the command line the C runtime asks for. */
void win32_init(int argc, char **argv);

/* -- peload.c: the mapped image, for the API functions that read it ------- */

extern const char *exe_path;
extern uint8_t *image;
extern uint32_t image_size;

/** [rva, rva + len) of the image; a bad range exits (corrupt .exe). */
uint8_t *image_at(uint64_t rva, uint64_t len);

/** Resource @p type / @p id (neutral or English if present) and its size,
 * or NULL. */
const uint8_t *find_resource(uint32_t type, uint32_t id, uint32_t *size);

/** Fill the delay-load IAT slot @p thunk described by @p desc. */
void *resolve_delay_import(const void *desc, void *thunk);

static inline uint16_t u16(const uint8_t *p) { return p[0] | p[1] << 8; }
static inline uint32_t u32(const uint8_t *p) {
  return u16(p) | (uint32_t)u16(p + 2) << 16;
}
static inline uint64_t u64(const uint8_t *p) {
  return u32(p) | (uint64_t)u32(p + 4) << 32;
}
