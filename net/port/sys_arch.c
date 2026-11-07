/*
 * What lwIP needs from the kernel with NO_SYS: a millisecond clock, random
 * numbers and an assert handler.
 */
#include "lwip/sys.h"

#include <hpet/hpet.h>
#include <hubble/printk.h>
#include <hubble/random.h>

u32_t sys_now(void) { return (u32_t)(hpet_get_time_ns() / 1000000); }

uint32_t net_rand(void) {
  uint32_t r;
  get_random_bytes(&r, sizeof(r));
  return r;
}

void net_lwip_assert(const char *msg, const char *file, int line) {
  printk(KERN_EMERG "lwip: assertion \"%s\" failed at %s:%d\n", msg, file,
         line);
  for (;;)
    __asm__ volatile("cli; hlt");
}
