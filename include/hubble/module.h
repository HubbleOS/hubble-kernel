#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <hubble/init.h>

#define __init
#define __exit

#define module_init(fn) \
	initcall_t __module_init_##fn \
	__attribute__((section(".module.init"), used)) = fn

#define module_exit(fn) \
	void (*__module_exit_##fn)(void) \
	__attribute__((section(".module.exit"), used)) = fn

#define MODULE_NAME(name) \
	static const char __module_name[] \
	__attribute__((section(".module.name"), used)) = name

#define MODULE_LICENSE(license) \
	static const char __module_license[] \
	__attribute__((section(".module.license"), used)) = license

#define MODULE_AUTHOR(author) \
	static const char __module_author[] \
	__attribute__((section(".module.author"), used)) = author

#define MODULE_DESCRIPTION(desc) \
	static const char __module_description[] \
	__attribute__((section(".module.desc"), used)) = desc

#define MODULE_DEPENDS(deps) \
	static const char __module_depends[] \
	__attribute__((section(".module.depends"), used)) = deps

int module_load(const char *path);
int module_load_buffer(const void *image, size_t size);
int module_load_directory(const char *path);
int module_unload(const char *name);
bool module_is_loaded(const char *name);
int module_refresh_symbols(void);

typedef struct module module_t;
module_t *module_find(const char *name);
