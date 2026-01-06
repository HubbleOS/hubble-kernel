#pragma once

#if defined(__x86_64__)
#include "arch/x86/syscall.h"
#elif defined(__aarch64__)
#include "arch/arm64/syscall.h"
#else
#error "Unsupported architecture"
#endif
