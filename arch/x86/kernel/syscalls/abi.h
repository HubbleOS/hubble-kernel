/**
 * @file abi.h
 * @brief Syscall personalities: which OS's syscalls a task speaks
 *
 * Every user task points at one ABI. exec picks it from the binary (the
 * ELF e_ident[EI_OSABI] byte) and fork inherits it, so programs built
 * for different systems can run side by side, each with its own syscall
 * numbering, argument registers and error convention.
 *
 * Windows programs are not an ABI here: they never make syscalls
 * themselves but call DLLs, which the user-space PE loader (/bin/peload)
 * provides on top of the Linux ABI.
 */
#pragma once

#include <stdint.h>

typedef struct registers registers_t;

typedef struct syscall_abi {
  const char *name;
  /** Handle the `syscall` in @p regs; the return value goes to RAX. */
  uint64_t (*dispatch)(registers_t *regs);
} syscall_abi_t;

extern const syscall_abi_t abi_linux;

/** @brief ABI for an ELF with this EI_OSABI byte, or NULL if unsupported */
const syscall_abi_t *abi_for_elf(uint8_t osabi);
