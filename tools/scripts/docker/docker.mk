# docker.mk

UNAME_S := $(shell uname -s)

WIN_NAMES := CYGWIN MINGW MSYS

IS_WIN := $(filter-out ,$(foreach w,$(WIN_NAMES),$(findstring $(w),$(UNAME_S))))

ifeq ($(IS_WIN),)
  DOCKER_RUN := docker-compose run --rm $(ARCH)-builder
else
  DOCKER_RUN := powershell.exe -File $(SCRIPT_DIR)/docker/docker-run.ps1 $(ARCH)-builder
endif

IS_WSL := $(findstring Microsoft,$(UNAME_S))

ifeq ($(IS_WSL),Microsoft)
  $(warning ⚠️  You are running inside WSL. Please ensure Docker Desktop's WSL 2 integration is enabled:)
  $(warning https://docs.docker.com/docker-for-windows/wsl/)
endif

DOCKER_TARGETS := build run
PHONY += $(patsubst %,docker-%,$(DOCKER_TARGETS))
PHONY += docker-%

docker-build:
	@echo "🛠️  Building kernel inside Docker..."
	@$(DOCKER_RUN) make build

docker-run: docker-build
	@make host-run

docker-%:
ifneq (,$(filter $*,$(DOCKER_CMDS)))
	@echo "🛠️  Running '$*' inside Docker..."
	@${DOCKER_RUN} make $*
else
	@make $*
endif
