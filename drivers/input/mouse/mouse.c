#include <drivers/input/serio/mouse.h>
#include <fs/vfs/dev.h>
#include <hubble/init.h>

static int mouse__init(void)
{
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file);
	return 0;
}

device_initcall(mouse__init);
