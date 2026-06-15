#include <hubble/module.h>
#include <hubble/errno.h>
#include <hubble/printk.h>

long sys_module_load(const char *path)
{
	if (!path)
		return -EINVAL;

	return module_load(path);
}
