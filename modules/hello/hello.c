#include <hubble/init.h>
#include <hubble/printk.h>

static int hello_module_init(void)
{
	printk(KERN_INFO "[hello] module loaded\n");
	return 0;
}

initcall(hello_module_init);
