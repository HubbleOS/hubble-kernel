/**
 * @file mouse.c
 * @brief PS/2 mouse VFS device registration
 */
#include <hubble/module.h>
#include <fs/vfs/dev.h>
#include <drivers/input/serio/mouse.h>

static int mouse__init(void)
{
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file, NULL);
	return 0;
}

module_init(mouse__init);
MODULE_NAME("mouse");
