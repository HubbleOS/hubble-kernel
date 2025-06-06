CONFIG_MK := $(abspath tools/config/config.mk)

include $(CONFIG_MK)

# export ARCH
# export OUT_DIR
# export BUILD_DIR
# export ISO_DIR
# export ARCH_DIR
# export LIBS_DIR
# export LIBC_DIR
# export CONFIG_MK
# export TOOLS_DIR
# export SCRIPT_DIR
# export INCLUDES

INCLUDES := \
	-I $(abspath include) \
	-I $(LIBC_DIR)/include \
	-I $(ARCH_DIR)/include 

PHONY := all
all: build

$(OUT_DIR)/$(ARCH)/gnu-efi/.built:
	$(MAKE) -C $(ARCH_DIR)/gnu-efi
	@mkdir -p $(dir $@)
	@touch $@

PHONY += build
build: $(OUT_DIR)/$(ARCH)/gnu-efi/.built
	@echo "🛠️  Building kernel for $(ARCH)..."
	$(MAKE) -C $(ARCH_DIR) \
		BUILD_DIR=$(BUILD_DIR) \
		ISO_DIR=$(ISO_DIR) \
		CONFIG_MK=$(CONFIG_MK) \
		LIBS_DIR=$(LIBS_DIR) \
		INCLUDES="$(INCLUDES)" 
	@echo "✅ Build complete for $(ARCH)"

PHONY += run
run: build
	@echo "🚀 Running kernel for $(ARCH)..."
	$(MAKE) host-run

PHONY += host-run
host-run:
	@echo "🖥  Launching QEMU from host..."
	$(MAKE) $(SCRIPT_DIR) qemu

PHONY += clean
clean:
	@echo "🧹 Cleaning build output for $(ARCH)..."
	@rm -rf $(OUT_DIR)
	@echo "✅ Clean complete"


DOCKER_TARGETS := build run
PHONY += $(patsubst %,docker-%,$(DOCKER_TARGETS))
PHONY += docker-%

PHONY += help
help:
	@echo "🧰 Kernel Build System"
	@echo ""
	@echo "📦 Usage:"
	@echo "  make [TARGET] [ARCH=<arch>] [VARIABLE=value]"
	@echo ""
	@echo "🎯 Targets:"
	@echo "  all            - Build the kernel (default)"
	@echo "  build          - Build the kernel"
	@echo "  run            - Run the built kernel via QEMU (on host)"
	@echo "  qemu           - Run QEMU using current build "
	@echo "  img            - Build the kernel image (if implemented)"
	@echo "  flash          - Flash the kernel image to a USB device (if implemented)"
	@echo "  clean          - Remove all build output"
	@echo "  mkvars         - Print key build variables (debug info)"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "🐳 Docker Targets:"
	@echo "  docker-build   - Build the kernel inside Docker container"
	@echo "  docker-run     - Build in Docker, run kernel on host QEMU"
	@echo "  docker-clean   - Clean build output via Docker"
	@echo "  docker-<target>- Run any target inside Docker, e.g., 'make docker-img'"
	@echo ""
	@echo "🖥  Host-only Targets:"
	@echo "  host-run       - Run QEMU from host using current build output"
	@echo ""
	@echo "🛠  Variables:"
	@echo "  ARCH           - Target architecture (e.g. x86, arm64). Default: x86"
	@echo "  DOCKER_RUN     - Override Docker run command if needed"
	@echo ""
	@echo "✅ Available ARCH values:"
	@echo "  x86, arm64 (extendable in config/config.mk and docker-compose.yml)"
	@echo ""

PHONY += mkvars
mkvars:
	@echo "📦 Build Variables:"
	@echo "  ARCH         = $(ARCH)"
	@echo "  OUT_DIR      = $(OUT_DIR)"
	@echo "  BUILD_DIR    = $(BUILD_DIR)"
	@echo "  ISO_DIR      = $(ISO_DIR)"
	@echo "  ARCH_DIR     = $(ARCH_DIR)"
	@echo "  LIBS_DIR     = $(LIBS_DIR)"
	@echo "  LIBC_DIR     = $(LIBC_DIR)"
	@echo "  CONFIG_MK    = $(CONFIG_MK)"
	@echo "  TOOLS_DIR    = $(TOOLS_DIR)"
	@echo "  SCRIPT_DIR   = $(SCRIPT_DIR)"
	@echo "  INCLUDES     = $(INCLUDES)"
	@echo "  DOCKER_RUN   = $(DOCKER_RUN)"

include $(SCRIPT_DIR)/scripts.mk
include $(SCRIPT_DIR)/docker.mk

.PHONY: $(PHONY)