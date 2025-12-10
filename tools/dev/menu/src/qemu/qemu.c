#include "qemu.h"

QemuConfig qemu_config = {
    .iso = "../../../../out/build/x86/iso/",
    .arch = "x86_64",
    .mem = 512,
    .smp = 2,
    .debug_port = 1000,
};
