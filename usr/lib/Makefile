# usr/lib/Makefile

subdirs := libc libgui

.PHONY: all
all:
	@set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir BUILD_TOOL_FLAGS="$(BUILD_TOOL_FLAGS)"; \
	done
