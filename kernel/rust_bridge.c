#include <hubble/printk.h>

void rust_printk(const char *msg) {
  printk("<6>%s", msg);
}
