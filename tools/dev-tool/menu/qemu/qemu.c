#include <qemu/qemu.h>

QemuConfig qemu_config = {
    .iso = "../../../out/x86/iso/",
    .arch = "x86_64",
    .mem = 1024,
    .smp = 2,
    .debug_port = 1234,
};
