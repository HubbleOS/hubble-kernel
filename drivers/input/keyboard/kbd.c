#include <dev/keyboard.h>
#include <fs/vfs/dev.h>
#include <hubble/init.h>

static int kbd_init(void)
{
	dev_vfs_register("kbd", NULL, kbd_read);
	return 0;
}

device_initcall(kbd_init);
