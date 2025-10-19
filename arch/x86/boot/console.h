#pragma once

#include <efi.h>
#include <efilib.h>

#define PrintOk(fmt, ...) PrintMessage(L"OK", EFI_LIGHTGREEN, fmt, ##__VA_ARGS__)
#define PrintError(fmt, ...) PrintMessage(L"ERROR", EFI_LIGHTRED, fmt, ##__VA_ARGS__)
#define PrintWarn(fmt, ...) PrintMessage(L"WARN", EFI_YELLOW, fmt, ##__VA_ARGS__)
#define PrintInfo(fmt, ...) PrintMessage(L"INFO", EFI_LIGHTBLUE, fmt, ##__VA_ARGS__)
#define PrintDebug(fmt, ...) PrintMessage(L"DEBUG", EFI_CYAN, fmt, ##__VA_ARGS__)

void PrintMessage(const CHAR16 *tag, UINTN color, const CHAR16 *fmt, ...);
