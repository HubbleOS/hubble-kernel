# scripts.mk

.PHONY: img flash qemu

img:
	@sh $(SCRIPT_DIR)/img/make-image.sh

flash:
	@sh $(SCRIPT_DIR)/img/make-flash.sh

qemu:
	@sh $(SCRIPT_DIR)/qemu/qemu.sh $(ISO_DIR)
