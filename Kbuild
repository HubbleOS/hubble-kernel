# Root Kbuild — Top-level module graph
#
# This file declares the kernel module hierarchy using Kbuild conventions.
# Each entry is a directory containing a module.mk (or Kbuild) file.
#
# Build order respects declaration order: modules are built left-to-right.
# Libraries (lib-y) and objects (obj-y) must precede the executable (exe-y)
# that links them.
#
# Architecture selection is directory-level only:
#   obj-y += arch/$(ARCH)/
#   → arch/x86/Kbuild or arch/arm64/Kbuild handles subdirectories.

# ---------------------------------------------------------------------------
# Kernel core objects (compiled to .o, consumed by final link)
# ---------------------------------------------------------------------------
obj-y += init/
obj-y += kernel/

# ---------------------------------------------------------------------------
# Static libraries (compiled to .a, linked into kernel.elf)
# ---------------------------------------------------------------------------
lib-y += net/
lib-y += fs/
lib-y += drivers/
lib-y += sound/

# `lib/` is a bridge — its Makefile declares subdir-y for its children
lib-y += lib/fonts/
lib-y += lib/color/
lib-y += lib/core/

# ---------------------------------------------------------------------------
# Loadable kernel modules
# ---------------------------------------------------------------------------
mod-y += modules/hello/

# ---------------------------------------------------------------------------
# Architecture: selected by ARCH variable
# ---------------------------------------------------------------------------
ifeq ($(ARCH),arm64)
    # arch/arm64/ is a bridge — Makefile declares subdir-y for kernel/
    obj-y += arch/arm64/
else
    # arch/x86/ is a bridge — Makefile declares subdir-y for kernel/
    obj-y += arch/x86/
endif

# ---------------------------------------------------------------------------
# Kernel executable (final link)
# ---------------------------------------------------------------------------
# The exe-y entry is in arch/$(ARCH)/kernel/module.mk because it needs
# arch-specific linker script and library groupings.
