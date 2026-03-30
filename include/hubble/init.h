#pragma once

typedef int (*initcall_t)(void);

#define __init
#define module_init(fn) \
	initcall_t __initcall_##fn __attribute__((section(".initcalls"), used)) = fn
