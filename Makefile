# Main Makefile
Q = @
export Q

ROOT_DIR := $(abspath .)
export ROOT_DIR

ARCH ?= x86
OUT_DIR ?= $(abspath out)
BUILD_DIR := $(OUT_DIR)/build/$(ARCH)
ARCH_DIR := $(abspath arch/$(ARCH))

TOOLS_DIR := tools
DEV_TOOLS_DIR := tools/dev
BUILD_TOOL := $(OUT_DIR)/tools/dev/build/build_main

SUPPORTED_ARCHES := x86 x86_64 arm64

ifneq ($(ARCH),$(filter $(ARCH),$(SUPPORTED_ARCHES)))
  $(error Unsupported architecture: $(ARCH). Supported: $(SUPPORTED_ARCHES))
endif

# Build mode: set RELEASE=1 for optimized builds without debug symbols
RELEASE ?= 0

# Parallel jobs: defaults to number of CPUs
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Compiler flags

ifeq ($(RELEASE),1)
CFLAGS = -ffreestanding -O3 -Wall -Wextra
else
CFLAGS = -ffreestanding -O2 -Wall -Wextra -g
endif

ifeq ($(ARCH),x86)
    CFLAGS += -mcmodel=kernel -m64 -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-avx -mno-avx2 -mno-red-zone
endif

ifeq ($(ARCH),arm64)
    CFLAGS += -march=armv8-a
endif

ifeq ($(ARCH),x86)
	CROSS   = x86_64-elf-
	CC      = $(CROSS)gcc
	AS      = $(CROSS)as
	LD      = $(CROSS)ld
	AR      = $(CROSS)ar
	OBJCOPY = $(CROSS)objcopy
	
	ASM     = nasm
	CFLAGS += -m64
	ASMFLAGS := -g -f elf64
endif
ifeq ($(ARCH),arm64)
    CROSS = aarch64-elf-
    CC    = $(CROSS)gcc
    AS    = $(CROSS)as
    LD    = $(CROSS)ld
    AR    = $(CROSS)ar
    OBJCOPY = $(CROSS)objcopy
    ASMFLAGS := -g -march=armv8-a
    ASM := $(AS)
endif


export LD CC AS AR OBJCOPY ASM CFLAGS ASMFLAGS
export ARCH OUT_DIR BUILD_DIR ARCH_DIR TOOLS_DIR BUILD_TOOL

INCLUDES += -I$(abspath .)
INCLUDES += -I$(abspath include)
INCLUDES += -I$(ARCH_DIR)/include
INCLUDES += -I$(ARCH_DIR)/kernel
export INCLUDES

LIB_DIR := $(abspath lib)
export LIB_DIR

LOG_DIR  := $(OUT_DIR)/logs/$(shell date +%Y-%m-%d)
LOG_FILE := $(LOG_DIR)/$(shell date +%H-%M-%S).log
export LOG_DIR LOG_FILE

ISO_DIR := $(BUILD_DIR)/iso
export ISO_DIR

ifeq ($(ARCH),x86)
EFI_NAME := BOOTx64.EFI
EFI_TARGET := efi-app-x86_64
endif

ifeq ($(ARCH),arm64)
EFI_NAME := BOOTAA64.EFI
EFI_TARGET := efi-app-aarch64
endif

# Libraries
# The path is displayed automatically: $(BUILD_DIR)/<relpath>/lib<name>.a

define lib-path
$(BUILD_DIR)/$(1)/lib$(notdir $(1)).a
endef

NET_LIB     := $(call lib-path,net)
FS_LIB      := $(call lib-path,fs)
DRIVERS_LIB := $(call lib-path,drivers)
SOUND_LIB   := $(call lib-path,sound)
FONT_LIB    := $(call lib-path,lib/fonts)
COLOR_LIB   := $(call lib-path,lib/color)
CORE_LIB    := $(call lib-path,lib/core)

export NET_LIB FS_LIB DRIVERS_LIB FONT_LIB COLOR_LIB CORE_LIB

LIBS := $(NET_LIB) $(FS_LIB) $(SOUND_LIB) $(DRIVERS_LIB) $(FONT_LIB) $(COLOR_LIB) $(CORE_LIB)
export LIBS

# kernel/ common objects

KCOMMON_BUILD_DIR := $(BUILD_DIR)/kernel

ifeq ($(ARCH),x86)
    KCOMMON_OBJS += $(BUILD_DIR)/init/main.o
    KCOMMON_OBJS += $(KCOMMON_BUILD_DIR)/module.o
    KCOMMON_OBJS += $(KCOMMON_BUILD_DIR)/device/device.o
    KCOMMON_OBJS += $(KCOMMON_BUILD_DIR)/init/fs.o
    KCOMMON_OBJS += $(KCOMMON_BUILD_DIR)/printk.o
    KCOMMON_OBJS += $(KCOMMON_BUILD_DIR)/syscalls/syscall.o
    KCOMMON_OBJS += $(KCOMMON_BUILD_DIR)/syscalls/sys_module.o
endif

export KCOMMON_BUILD_DIR KCOMMON_OBJS

# Build tool flags

BUILD_TOOL_FLAGS = --log-file $(LOG_FILE) -v --jobs $(JOBS)

# Build macros

# $(d) = relpath, expanded when called via foreach
define build-lib-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--cflags    "$(CFLAGS) $(ccflags-y)" \
		--includes  "$(INCLUDES)" \
		--ar $(AR) \
		--output    $(BUILD_DIR)/$(d)/lib$(notdir $(d)).a \
		--type archive
endef

define build-lib-asm-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(CFLAGS) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--ar $(AR) \
		--output    $(BUILD_DIR)/$(d)/lib$(notdir $(d)).a \
		--type archive
endef

define build-obj-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(CFLAGS) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--type objects
endef

define build-exe-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(if $(cflags-y),$(cflags-y),$(CFLAGS)) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--output    $(exe-output-y) \
		--type exe \
		--ld $(LD) \
		$(if $(exe-ldflags-y), --ldflags  "$(exe-ldflags-y)") \
		$(if $(exe-objs-y),    --obj-files "$(exe-objs-y)") \
		$(if $(exe-libs-y),    --libs      "$(exe-libs-y)")
endef

define build-mod-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(if $(cflags-y),$(cflags-y),$(CFLAGS)) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--output    $(mod-output-y) \
		--type module \
		--ld $(LD) \
		$(if $(mod-ldflags-y), --ldflags  "$(mod-ldflags-y)") \
		$(if $(mod-objs-y),    --obj-files "$(mod-objs-y)") \
		$(if $(mod-libs-y),    --libs      "$(mod-libs-y)")
endef

define reset-module-vars
	$(eval lib-y        :=)
	$(eval lib-asm-y    :=)
	$(eval obj-y        :=)
	$(eval exe-y        :=)
	$(eval exe-output-y :=)
	$(eval exe-ldflags-y :=)
	$(eval exe-objs-y   :=)
	$(eval exe-libs-y   :=)
	$(eval mod-y        :=)
	$(eval mod-output-y :=)
	$(eval mod-ldflags-y :=)
	$(eval mod-objs-y   :=)
	$(eval mod-libs-y   :=)
	$(eval subdir-y     :=)
	$(eval always-y     :=)
	$(eval ccflags-y    :=)
	$(eval asflags-y    :=)
	$(eval cppflags-y   :=)
	$(eval ldflags-y    :=)
endef

define load-module
	$(if $(wildcard $(ROOT_DIR)/$(1)/Makefile),  $(eval include $(ROOT_DIR)/$(1)/Makefile)) \
	$(if $(wildcard $(ROOT_DIR)/$(1)/module.mk), $(eval include $(ROOT_DIR)/$(1)/module.mk)) \
	$(if $(wildcard $(ROOT_DIR)/$(1)/Makefile)$(wildcard $(ROOT_DIR)/$(1)/module.mk), \
		$(foreach d,$(lib-y),     $(call build-lib-module)) \
		$(foreach d,$(lib-asm-y), $(call build-lib-asm-module)) \
		$(foreach d,$(obj-y),     $(call build-obj-module)) \
		$(foreach d,$(exe-y),     $(call build-exe-module)) \
		$(foreach d,$(mod-y),     $(call build-mod-module)) \
		$(eval _subdirs_$(subst /,_,$(1)) := $(subdir-y)) \
		$(call reset-module-vars) \
		$(foreach s,$(_subdirs_$(subst /,_,$(1))),$(call load-module,$(s))) \
	, \
		$(error no module.mk or Makefile found in: $(ROOT_DIR)/$(1)) \
	)
endef

# Module list

MODULES :=

ifeq ($(ARCH),arm64)
    MODULES += arch/arm64
else 
    MODULES += init
    MODULES += net
    MODULES += fs
	    MODULES += drivers
	    MODULES += lib
	    MODULES += sound
	    MODULES += kernel
	    MODULES += modules
	    MODULES += arch/$(ARCH)
endif

# arch subdirs (boot etc.)

define kbuild-subdir
  subdir-y :=
  include $(1)/Makefile
  subdirs += $(addprefix $(1)/, $(subdir-y))
  subdir-y :=
endef

subdirs :=
$(eval $(call kbuild-subdir,arch/$(ARCH)))
$(eval $(call kbuild-subdir,tools/dev))

USR_DIR := $(abspath usr)
export USR_DIR

USR_BUILD :=
ifneq ($(ARCH),arm64)
USR_BUILD := $(MAKE) -C $(USR_DIR) -j$(JOBS)
endif

# Targets

PHONY += build-tool
build-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/build build

PHONY += build
build: build-tool
	@mkdir -p $(LOG_DIR)
	$(foreach mod,$(MODULES),$(call load-module,$(mod)))
	$(Q)set -e; for dir in $(filter-out arch/$(ARCH)/kernel,$(subdirs)); do \
		$(MAKE) -C $$dir; \
	done
	$(USR_BUILD)
	@echo "Build complete"

PHONY += run
run: build
	$(MAKE) -C arch/$(ARCH)/boot
	@python3 tools/dev/qemu/main.py

PHONY += disk
disk:
	@mkdir -p out/disks
	@python3 tools/dev/disk/main.py

PHONY += flash
flash:
	@python3 tools/dev/flash/main.py

PHONY += demo
demo:
	@$(MAKE) -C tools/dev/demo run

PHONY += clean
clean:
	@rm -rf $(OUT_DIR)
	@echo "Clean complete"

PHONY += rebuild
rebuild: clean build

PHONY += mkvars
mkvars:
	@echo "ARCH         = $(ARCH)"
	@echo "BUILD_DIR    = $(BUILD_DIR)"
	@echo "LIBS         = $(LIBS)"
	@echo "KCOMMON_OBJS = $(KCOMMON_OBJS)"
	@echo "MODULES      = $(MODULES)"
	@echo "subdirs      = $(subdirs)"

PHONY += help
help:
	@echo "Usage: make [TARGET] [ARCH=<arch>]"
	@echo "Targets: build, run, disk, clean, rebuild, mkvars"
	@echo "Arches:  x86, x86_64, arm64"

.PHONY: $(PHONY)
