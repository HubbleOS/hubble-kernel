# Scripts Makefile
SCIPTS_DIR := scripts
RUN_SCRIPT := $(SCIPTS_DIR)/run.sh
SH = sh
RUN = $(SH) $(RUN_SCRIPT)

.PHONY: img flash help qemu

TARGETS := img flash help qemu

$(TARGETS):
	@$(RUN) $@
