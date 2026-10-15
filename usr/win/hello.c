/*
 * hello.exe - a Windows console program for testing /bin/peload.
 *
 * Built with x86_64-w64-mingw32-gcc -nostdlib (usr/Makefile): no C
 * runtime, so it imports only the kernel32 functions it calls.
 */
#include <windows.h>

/* Writable and global, so gcc cannot fold it into RIP-relative code:
 * the pointers stay absolute addresses in .data, and the linker emits
 * base relocations for them, which peload must apply. */
const char *lines[] = {
    "hello from a Windows program\r\n",
    "running on HubbleOS through peload\r\n",
};

static DWORD length(const char *s) {
  DWORD n = 0;
  while (s[n])
    n++;
  return n;
}

void start(void) {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD written;
  for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    WriteFile(out, lines[i], length(lines[i]), &written, NULL);
  ExitProcess(0);
}
