#include <stdarg.h>

#include "globals.h"
#include "console.h"

// void PrintMessage(const CHAR16 *tag, UINTN color, const CHAR16 *fmt, ...)
// {
// 	va_list args;
// 	va_start(args, fmt);

// 	// Output "[ TAG ]" with color
// 	Print(L"[ ");
// 	uefi_call_wrapper(g_systab->ConOut->SetAttribute, 2, g_systab->ConOut, color);
// 	Print(tag);
// 	uefi_call_wrapper(g_systab->ConOut->SetAttribute, 2, g_systab->ConOut, EFI_LIGHTGRAY);
// 	Print(L" ] ");

// 	// Output message
// 	VPrint(fmt, args);

// 	va_end(args);
// }

void PrintMessage(const CHAR16 *tag, UINTN color, const CHAR16 *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	const UINTN tag_width = 7; // desired tag width inside brackets
	UINTN tag_len = StrLen(tag);

	// calculate indents
	UINTN total_padding = (tag_width > tag_len) ? (tag_width - tag_len) : 0;
	UINTN pad_left = total_padding / 2;
	UINTN pad_right = total_padding - pad_left;

	Print(L"[ ");

	// tag color
	uefi_call_wrapper(g_systab->ConOut->SetAttribute, 2, g_systab->ConOut, color);

	// left indent
	for (UINTN i = 0; i < pad_left; i++)
		Print(L" ");

	// tag
	Print(tag);

	// right indent
	for (UINTN i = 0; i < pad_right; i++)
		Print(L" ");

	// reset color
	uefi_call_wrapper(g_systab->ConOut->SetAttribute, 2, g_systab->ConOut, EFI_LIGHTGRAY);

	Print(L" ] ");

	// message
	VPrint(fmt, args);

	va_end(args);
}
