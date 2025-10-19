#include <stdarg.h>

#include "globals.h"
#include "console.h"

void PrintMessage(const CHAR16 *tag, UINTN color, const CHAR16 *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	// Output "[ TAG ]" with color
	Print(L"[ ");
	uefi_call_wrapper(g_systab->ConOut->SetAttribute, 2, g_systab->ConOut, color);
	Print(tag);
	uefi_call_wrapper(g_systab->ConOut->SetAttribute, 2, g_systab->ConOut, EFI_LIGHTGRAY);
	Print(L" ] ");

	// Output message
	VPrint(fmt, args);

	va_end(args);
}
