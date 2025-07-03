LIBC_DIR := libc

subdirs += $(LIBC_DIR)

.PHONY: build
build: 
	$(Q)set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir; \
	done
