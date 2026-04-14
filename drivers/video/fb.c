#include <hubble/platform.h>
#include <hubble/init.h>
#include <fs/vfs/dev.h>

static uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return g_platform.fb_base;
}

static int fb_init(void)
{
	dev_vfs_register("fb0", fb_mmap, NULL, NULL);
	return 0;
}

device_initcall(fb_init);
