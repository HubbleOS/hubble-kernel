extern int main(void);

void _start(void)
{
	int ret = main();
	(void)ret;
	// бесконечный цикл после main
	// while (1)
	// __asm__("hlt");
}
