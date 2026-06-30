#pragma once

/**
 * @brief Kernel initcall infrastructure.
 *
 * Initcalls are constructor-style functions placed into named ELF sections.
 * They are executed in dependency order during boot: early, core, fs,
 * device, late.
 */

typedef int (*initcall_t)(void);

#define __init

#define early_initcall(fn) \
	initcall_t __initcall_##fn \
	__attribute__((section(".initcalls.early"), used)) = fn

#define initcall(fn) \
	initcall_t __initcall_##fn \
	__attribute__((section(".initcalls.core"), used)) = fn

#define fs_initcall(fn) \
	initcall_t __initcall_##fn \
	__attribute__((section(".initcalls.fs"), used)) = fn

#define device_initcall(fn) \
	initcall_t __initcall_##fn \
	__attribute__((section(".initcalls.device"), used)) = fn

#define late_initcall(fn) \
	initcall_t __initcall_##fn \
	__attribute__((section(".initcalls.late"), used)) = fn
