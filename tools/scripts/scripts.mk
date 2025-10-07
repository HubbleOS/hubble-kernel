# scripts.mk

.PHONY: img flash

img:
	@sh $(SCRIPT_DIR)/img/make-image.sh

flash:
	@sh $(SCRIPT_DIR)/img/make-flash.sh
