/*
 * win32.c - the Windows API peload gives programs, on top of Linux.
 *
 * Each DLL is a table of the functions below; find_import() looks them
 * up by name. Adding a function is one WINAPI function plus one table
 * line. What is here is what real console programs (e.g. Wine's
 * hostname.exe) need: kernel32 basics, the start-up and printf half of
 * the C runtime (ucrtbase.dll), and user32's LoadStringW.
 *
 * Strings: Windows "W" functions take UTF-16 (WCHAR); we convert to and
 * from UTF-8 at the edges, and treat every code page as UTF-8.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "peload.h"

typedef uint32_t DWORD;
typedef int BOOL;
typedef uint16_t WCHAR;
typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define ERROR_INVALID_HANDLE 6
#define ERROR_NOT_SUPPORTED 50
#define ERROR_INVALID_PARAMETER 87
#define ERROR_BUFFER_OVERFLOW 111
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_MOD_NOT_FOUND 126
#define ERROR_RESOURCE_NAME_NOT_FOUND 1814

/* -- Process and thread blocks ---------------------------------------------
 *
 * Windows code finds its thread's TEB at %gs:0x30, and through it the
 * process's PEB: the C runtime reads the image base from it, programs
 * the default heap and GetLastError's value. We fill in what programs
 * read; the rest stays zero. */

#define TEB_SIZE 0x2000 /* past the TLS slots at 0x1480 */
#define TEB_SELF 0x30
#define TEB_PID 0x40
#define TEB_TID 0x48
#define TEB_PEB 0x60
#define TEB_LAST_ERROR 0x68
#define PEB_SIZE 0x1000
#define PEB_IMAGE_BASE 0x10
#define PEB_PROCESS_HEAP 0x30

#define ARCH_SET_GS 0x1001

static uint8_t *teb;
static char process_heap; /* only its address matters: the heap handle */

static void put64(uint8_t *at, uint64_t v) { memcpy(at, &v, 8); }

static void set_last_error(DWORD e) { memcpy(teb + TEB_LAST_ERROR, &e, 4); }

/* -- UTF-16 <-> UTF-8 ------------------------------------------------------ */

/* Length of a NUL-terminated UTF-16 string. */
static size_t wlen(const WCHAR *s) {
  size_t n = 0;
  while (s[n])
    n++;
  return n;
}

/* Convert @p n WCHARs to UTF-8 in @p out (NULL: just count). Returns the
 * UTF-8 length. Unpaired surrogates become U+FFFD. */
static size_t to_utf8(const WCHAR *s, size_t n, char *out) {
  size_t len = 0;
  for (size_t i = 0; i < n; i++) {
    uint32_t c = s[i];
    if (c >= 0xd800 && c < 0xdc00 && i + 1 < n && s[i + 1] >= 0xdc00 &&
        s[i + 1] < 0xe000)
      c = 0x10000 + ((c - 0xd800) << 10) + (s[++i] - 0xdc00);
    else if (c >= 0xd800 && c < 0xe000)
      c = 0xfffd;

    char b[4];
    size_t k;
    if (c < 0x80) {
      b[0] = c, k = 1;
    } else if (c < 0x800) {
      b[0] = 0xc0 | c >> 6, b[1] = 0x80 | (c & 0x3f), k = 2;
    } else if (c < 0x10000) {
      b[0] = 0xe0 | c >> 12, b[1] = 0x80 | (c >> 6 & 0x3f);
      b[2] = 0x80 | (c & 0x3f), k = 3;
    } else {
      b[0] = 0xf0 | c >> 18, b[1] = 0x80 | (c >> 12 & 0x3f);
      b[2] = 0x80 | (c >> 6 & 0x3f), b[3] = 0x80 | (c & 0x3f), k = 4;
    }
    if (out)
      memcpy(out + len, b, k);
    len += k;
  }
  return len;
}

/* Decode one UTF-8 sequence at *s (invalid bytes become U+FFFD). */
static uint32_t utf8_next(const unsigned char **s) {
  const unsigned char *p = *s;
  uint32_t c = *p++;
  int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
  if (c >= 0x80 && (c < 0xc0 || c >= 0xf8)) {
    *s = p;
    return 0xfffd;
  }
  c &= extra ? 0x3f >> extra : 0x7f;
  for (int i = 0; i < extra; i++, p++) {
    if ((*p & 0xc0) != 0x80) {
      *s = p;
      return 0xfffd;
    }
    c = c << 6 | (*p & 0x3f);
  }
  *s = p;
  return c;
}

/* A NUL-terminated UTF-8 string as a new UTF-16 one. */
static WCHAR *to_utf16(const char *s) {
  WCHAR *w = malloc((strlen(s) + 1) * sizeof(WCHAR)); /* never more units */
  if (!w) {
    fprintf(stderr, "peload: out of memory\n");
    exit(126);
  }
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p;) {
    uint32_t c = utf8_next(&p);
    if (c >= 0x10000) {
      c -= 0x10000;
      w[n++] = 0xd800 | c >> 10;
      c = 0xdc00 | (c & 0x3ff);
    }
    w[n++] = c;
  }
  w[n] = 0;
  return w;
}

/* -- kernel32.dll ----------------------------------------------------------
 *
 * A HANDLE for a file descriptor is fd + 1, so fd 0 is not NULL (which
 * Windows code treats as "no handle"). */

static HANDLE fd_handle(int fd) { return (HANDLE)(intptr_t)(fd + 1); }
static int handle_fd(HANDLE h) { return (int)(intptr_t)h - 1; }

static WINAPI HANDLE GetStdHandle(DWORD which) {
  switch ((int32_t)which) {
  case -10: return fd_handle(0); /* STD_INPUT_HANDLE */
  case -11: return fd_handle(1); /* STD_OUTPUT_HANDLE */
  case -12: return fd_handle(2); /* STD_ERROR_HANDLE */
  default: return INVALID_HANDLE_VALUE;
  }
}

static WINAPI BOOL WriteFile(HANDLE h, const void *buf, DWORD len,
                             DWORD *written, void *overlapped) {
  if (overlapped)
    return 0; /* async I/O: not supported */
  ssize_t n = write(handle_fd(h), buf, len);
  if (written)
    *written = n < 0 ? 0 : (DWORD)n;
  return n >= 0;
}

static WINAPI BOOL ReadFile(HANDLE h, void *buf, DWORD len, DWORD *nread,
                            void *overlapped) {
  if (overlapped)
    return 0;
  ssize_t n = read(handle_fd(h), buf, len);
  if (nread)
    *nread = n < 0 ? 0 : (DWORD)n;
  return n >= 0;
}

/* Only consoles take it; programs fall back to WriteFile when output is
 * redirected, as on Windows. */
static WINAPI BOOL WriteConsoleW(HANDLE h, const WCHAR *buf, DWORD n,
                                 DWORD *written, void *reserved) {
  (void)reserved;
  int fd = handle_fd(h);
  if (!isatty(fd)) {
    set_last_error(ERROR_INVALID_HANDLE);
    return 0;
  }
  size_t len = to_utf8(buf, n, NULL);
  char *s = malloc(len ? len : 1);
  if (!s)
    return 0;
  to_utf8(buf, n, s);
  for (size_t done = 0; done < len;) {
    ssize_t k = write(fd, s + done, len - done);
    if (k <= 0) {
      free(s);
      return 0;
    }
    done += k;
  }
  free(s);
  if (written)
    *written = n;
  return 1;
}

static WINAPI void ExitProcess(unsigned code) { exit((int)code); }

/* The NetBIOS name Windows reports: the host name up to the first dot,
 * at most 15 characters, upper case. */
static WINAPI BOOL GetComputerNameW(WCHAR *buf, DWORD *size) {
  struct utsname u;
  if (uname(&u) < 0) {
    set_last_error(ERROR_NOT_SUPPORTED);
    return 0;
  }
  size_t n = strcspn(u.nodename, ".");
  if (n > 15)
    n = 15;
  if (*size < n + 1) {
    *size = n + 1;
    set_last_error(ERROR_BUFFER_OVERFLOW);
    return 0;
  }
  for (size_t i = 0; i < n; i++) {
    unsigned char c = u.nodename[i];
    buf[i] = c >= 'a' && c <= 'z' ? c - 32 : c;
  }
  buf[n] = 0;
  *size = n;
  return 1;
}

/* Only the .exe itself is loaded: there are no DLL modules to name. */
static WINAPI void *GetModuleHandleW(const WCHAR *name) {
  if (!name)
    return image;
  set_last_error(ERROR_MOD_NOT_FOUND);
  return NULL;
}

#define CP_ACP 0
#define CP_OEMCP 1
#define CP_UTF8 65001

static WINAPI unsigned GetOEMCP(void) { return CP_UTF8; }
static WINAPI unsigned GetACP(void) { return CP_UTF8; }

static WINAPI int WideCharToMultiByte(unsigned cp, DWORD flags,
                                      const WCHAR *src, int srclen, char *dst,
                                      int dstlen, const char *defchar,
                                      BOOL *used_default) {
  (void)flags, (void)defchar;
  if ((cp != CP_ACP && cp != CP_OEMCP && cp != CP_UTF8) || !src ||
      dstlen < 0) {
    set_last_error(ERROR_INVALID_PARAMETER);
    return 0;
  }
  if (used_default)
    *used_default = 0;
  /* -1: up to and including the terminator */
  size_t n = srclen < 0 ? wlen(src) + 1 : (size_t)srclen;
  size_t len = to_utf8(src, n, NULL);
  if (dstlen == 0)
    return (int)len; /* just asking for the size */
  if (len > (size_t)dstlen) {
    set_last_error(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }
  to_utf8(src, n, dst);
  return (int)len;
}

#define HEAP_ZERO_MEMORY 0x8

static WINAPI void *HeapAlloc(HANDLE heap, DWORD flags, size_t size) {
  (void)heap;
  return flags & HEAP_ZERO_MEMORY ? calloc(1, size) : malloc(size);
}

static WINAPI BOOL HeapFree(HANDLE heap, DWORD flags, void *p) {
  (void)heap, (void)flags;
  free(p);
  return 1;
}

static WINAPI void *ResolveDelayLoadedAPI(void *base, const void *desc,
                                          void *dll_hook, void *system_hook,
                                          void *thunk, DWORD flags) {
  (void)dll_hook, (void)system_hook, (void)flags;
  if (base != image) {
    fprintf(stderr, "peload: %s: delay load for an unknown module\n",
            exe_path);
    exit(126);
  }
  /* A missing function exits with its name: no failure hook is called. */
  return resolve_delay_import(desc, thunk);
}

/* Only reached through a failure hook, which we never call. */
static WINAPI void *DelayLoadFailureHook(const char *dll, const char *fn) {
  (void)dll, (void)fn;
  return NULL;
}

static const struct func kernel32[] = {
    {"DelayLoadFailureHook", DelayLoadFailureHook},
    {"ExitProcess", ExitProcess},
    {"GetACP", GetACP},
    {"GetComputerNameW", GetComputerNameW},
    {"GetModuleHandleW", GetModuleHandleW},
    {"GetOEMCP", GetOEMCP},
    {"GetStdHandle", GetStdHandle},
    {"HeapAlloc", HeapAlloc},
    {"HeapFree", HeapFree},
    {"ReadFile", ReadFile},
    {"ResolveDelayLoadedAPI", ResolveDelayLoadedAPI},
    {"WideCharToMultiByte", WideCharToMultiByte},
    {"WriteConsoleW", WriteConsoleW},
    {"WriteFile", WriteFile},
    {NULL, NULL},
};

/* -- user32.dll ------------------------------------------------------------ */

#define RT_STRING 6

/* String resources come in blocks of 16, each string a WCHAR count then
 * that many WCHARs (no terminator); block n + 1 holds ids 16n..16n+15. */
static WINAPI int LoadStringW(void *module, unsigned id, WCHAR *buf, int max) {
  if (module && module != image)
    return 0;
  uint32_t size;
  const uint8_t *p = find_resource(RT_STRING, (id >> 4) + 1, &size);
  const uint8_t *end = p + size;
  for (unsigned i = 0; p && i < (id & 15); i++) {
    if (end - p < 2 || (size_t)(end - p - 2) / 2 < u16(p))
      p = NULL; /* corrupt block */
    else
      p += 2 + 2 * u16(p);
  }
  if (!p || end - p < 2 || (size_t)(end - p - 2) / 2 < u16(p) || !u16(p)) {
    set_last_error(ERROR_RESOURCE_NAME_NOT_FOUND);
    if (buf && max > 0)
      buf[0] = 0;
    return 0;
  }
  int len = u16(p);
  const WCHAR *s = (const WCHAR *)(p + 2);
  if (max == 0) { /* a read-only pointer to the string itself */
    memcpy(buf, &s, sizeof(s));
    return len;
  }
  if (!buf || max < 0)
    return 0;
  if (len > max - 1)
    len = max - 1;
  memcpy(buf, s, len * sizeof(WCHAR));
  buf[len] = 0;
  return len;
}

static const struct func user32[] = {
    {"LoadStringW", LoadStringW},
    {NULL, NULL},
};

/* -- ucrtbase.dll: process start-up ---------------------------------------
 *
 * The program's own start code asks the runtime for argc/argv and the
 * environment, runs its static constructors through _initterm, then
 * calls main and exit. */

static int win_argc;
static WCHAR **win_wargv;
static WCHAR **win_wenvp;

#define MAX_ATEXIT 32
static void(WINAPI *atexit_fns[MAX_ATEXIT])(void);
static int atexit_count;

/* Every way out (exit, ExitProcess, returning from the entry point) ends
 * in libc's exit, so register the program's handlers there once. */
static void run_atexit(void) {
  while (atexit_count > 0)
    atexit_fns[--atexit_count]();
}

static WINAPI void _set_app_type(int type) { (void)type; }
static WINAPI int _configure_wide_argv(int mode) { (void)mode; return 0; }
static WINAPI int _initialize_wide_environment(void) { return 0; }
static WINAPI int *__p___argc(void) { return &win_argc; }
static WINAPI WCHAR ***__p___wargv(void) { return &win_wargv; }
static WINAPI WCHAR **_get_initial_wide_environment(void) { return win_wenvp; }

typedef void(WINAPI *initterm_fn)(void);
typedef int(WINAPI *initterm_e_fn)(void);

static WINAPI void _initterm(initterm_fn *begin, initterm_fn *end) {
  for (; begin < end; begin++)
    if (*begin)
      (*begin)();
}

static WINAPI int _initterm_e(initterm_e_fn *begin, initterm_e_fn *end) {
  for (; begin < end; begin++) {
    int err = *begin ? (*begin)() : 0;
    if (err)
      return err;
  }
  return 0;
}

static WINAPI int _crt_atexit(void(WINAPI *fn)(void)) {
  if (atexit_count == MAX_ATEXIT)
    return -1;
  atexit_fns[atexit_count++] = fn;
  return 0;
}

static WINAPI void crt_exit(int code) { exit(code); }

static WINAPI int crt_wcsncmp(const WCHAR *a, const WCHAR *b, size_t n) {
  for (; n; n--, a++, b++)
    if (*a != *b || !*a)
      return *a < *b ? -1 : *a > *b;
  return 0;
}

/* -- ucrtbase.dll: wide printf ---------------------------------------------
 *
 * All the printf family ends in __stdio_common_vswprintf. Its va_list is
 * the Windows one: a pointer to the arguments, each in an 8-byte slot.
 * Numbers are formatted by our libc's snprintf from the same spec. */

#define PRINTF_STANDARD_SNPRINTF 0x2 /* overflow returns the full length */
#define PRINTF_LEGACY_WIDE 0x4       /* %s is a wide string, %hs narrow */

struct wout {
  WCHAR *buf;
  size_t cap, len; /* len counts past cap: the full length */
};

static void wput(struct wout *o, WCHAR c) {
  if (o->len < o->cap)
    o->buf[o->len] = c;
  o->len++;
}

static void wpad(struct wout *o, int n) {
  while (n-- > 0)
    wput(o, ' ');
}

static void format(struct wout *o, uint64_t options, const WCHAR *f,
                  const uint64_t *args) {
  while (*f) {
    if (*f != '%') {
      wput(o, *f++);
      continue;
    }
    const WCHAR *spec_start = f++;
    char spec[32] = "%";
    size_t sl = 1;
    bool left = false;

    while (*f && strchr("-+ #0", *f)) {
      left |= *f == '-';
      if (sl < 8)
        spec[sl++] = (char)*f;
      f++;
    }
    int width = -1, prec = -1;
    if (*f == '*') {
      width = (int)*args++, f++;
      if (width < 0)
        left = true, width = -width;
    } else if (*f >= '0' && *f <= '9') {
      for (width = 0; *f >= '0' && *f <= '9'; f++)
        width = width * 10 + (*f - '0');
    }
    if (*f == '.') {
      f++;
      if (*f == '*') {
        prec = (int)*args++, f++;
      } else {
        for (prec = 0; *f >= '0' && *f <= '9'; f++)
          prec = prec * 10 + (*f - '0');
      }
    }

    /* Size: h, l, ll, I64, I32, I, z, j, t. Only "is it 64-bit" and
     * "narrow or wide" matter here. */
    int size = 0; /* -1 h, 0 int, 1 long (32-bit on Windows), 2 64-bit */
    if (*f == 'h') {
      size = -1, f++;
      if (*f == 'h')
        f++;
    } else if (*f == 'l') {
      size = 1, f++;
      if (*f == 'l')
        size = 2, f++;
    } else if (*f == 'w') {
      size = 1, f++;
    } else if (f[0] == 'I' && f[1] == '6' && f[2] == '4') {
      size = 2, f += 3;
    } else if (f[0] == 'I' && f[1] == '3' && f[2] == '2') {
      size = 0, f += 3;
    } else if (*f == 'I' || *f == 'z' || *f == 'j' || *f == 't') {
      size = 2, f++;
    }

    WCHAR conv = *f ? *f++ : 0;
    switch (conv) {
    case '%':
      wput(o, '%');
      break;
    case 'c':
    case 'C': {
      wpad(o, left ? 0 : width - 1);
      wput(o, (WCHAR)*args++);
      wpad(o, left ? width - 1 : 0);
      break;
    }
    case 's':
    case 'S': {
      /* Which string type: the legacy Microsoft rule, unless h/l say. */
      bool wide = size == 1 || (size != -1 && ((conv == 's') ==
                                               !!(options & PRINTF_LEGACY_WIDE)));
      const void *s = (const void *)*args++;
      if (!s)
        s = wide ? (const void *)u"(null)" : "(null)";
      size_t n = wide ? wlen(s) : strlen(s);
      if (prec >= 0 && (size_t)prec < n)
        n = prec;
      wpad(o, left ? 0 : width - (int)n);
      if (wide) {
        for (size_t i = 0; i < n; i++)
          wput(o, ((const WCHAR *)s)[i]);
      } else { /* narrow strings are UTF-8 here */
        const unsigned char *p = s, *end = p + n;
        while (p < end) {
          uint32_t c = utf8_next(&p);
          wput(o, c < 0x10000 ? c : 0xfffd);
        }
      }
      wpad(o, left ? width - (int)n : 0);
      break;
    }
    case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p':
    case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'a':
    case 'A': {
      if (width >= 0)
        sl += snprintf(spec + sl, sizeof(spec) - sl, "%d", width);
      if (prec >= 0)
        sl += snprintf(spec + sl, sizeof(spec) - sl, ".%d", prec);
      uint64_t v = *args++;
      char num[512];
      if (strchr("eEfFgGaA", conv)) {
        double d;
        memcpy(&d, &v, 8);
        spec[sl++] = (char)conv, spec[sl] = 0;
        snprintf(num, sizeof(num), spec, d);
      } else if (conv == 'p') {
        snprintf(spec + sl, sizeof(spec) - sl, "016llX");
        snprintf(num, sizeof(num), spec, (unsigned long long)v);
      } else {
        snprintf(spec + sl, sizeof(spec) - sl, "ll%c", (char)conv);
        bool sign = conv == 'd' || conv == 'i';
        long long x = size == 2   ? (long long)v
                      : size == -1 ? (sign ? (long long)(int16_t)v
                                           : (long long)(uint16_t)v)
                      : sign      ? (long long)(int32_t)v
                                  : (long long)(uint32_t)v;
        snprintf(num, sizeof(num), spec, x);
      }
      for (char *c = num; *c; c++)
        wput(o, (unsigned char)*c);
      break;
    }
    default: /* unknown: print it as it was written */
      for (; spec_start < f; spec_start++)
        wput(o, *spec_start);
    }
  }
}

static WINAPI int __stdio_common_vswprintf(uint64_t options, WCHAR *buf,
                                           size_t count, const WCHAR *fmt,
                                           void *locale, const uint64_t *args) {
  (void)locale;
  if (!fmt)
    return -1;
  struct wout o = {buf, buf ? count : 0, 0};
  format(&o, options, fmt, args);
  if (o.len < o.cap) {
    buf[o.len] = 0;
    return (int)o.len;
  }
  if (!buf)
    return (int)o.len; /* counting only (_vscwprintf) */
  if (count)
    buf[count - 1] = 0;
  return options & PRINTF_STANDARD_SNPRINTF ? (int)o.len : -1;
}

static const struct func ucrtbase[] = {
    {"__p___argc", __p___argc},
    {"__p___wargv", __p___wargv},
    {"__stdio_common_vswprintf", __stdio_common_vswprintf},
    {"_configure_wide_argv", _configure_wide_argv},
    {"_crt_atexit", _crt_atexit},
    {"_get_initial_wide_environment", _get_initial_wide_environment},
    {"_initialize_wide_environment", _initialize_wide_environment},
    {"_initterm", _initterm},
    {"_initterm_e", _initterm_e},
    {"_set_app_type", _set_app_type},
    {"exit", crt_exit},
    {"wcsncmp", crt_wcsncmp},
    {NULL, NULL},
};

/* -- Lookup ---------------------------------------------------------------- */

static const struct {
  const char *name;
  const struct func *funcs;
} dlls[] = {
    {"kernel32.dll", kernel32},
    {"user32.dll", user32},
    {"ucrtbase.dll", ucrtbase},
};

void *find_import(const char *dll, const char *name) {
  for (size_t i = 0; i < sizeof(dlls) / sizeof(dlls[0]); i++) {
    if (strcasecmp(dlls[i].name, dll) != 0) /* DLL names ignore case */
      continue;
    for (const struct func *f = dlls[i].funcs; f->name; f++)
      if (strcmp(f->name, name) == 0) /* function names do not */
        return f->addr;
  }
  return NULL;
}

void win32_init(int argc, char **argv) {
  extern char **environ;

  teb = calloc(1, TEB_SIZE);
  uint8_t *peb = calloc(1, PEB_SIZE);
  if (!teb || !peb) {
    fprintf(stderr, "peload: out of memory\n");
    exit(126);
  }
  put64(teb + TEB_SELF, (uint64_t)(uintptr_t)teb);
  put64(teb + TEB_PID, (uint64_t)getpid());
  put64(teb + TEB_TID, (uint64_t)getpid()); /* single-threaded */
  put64(teb + TEB_PEB, (uint64_t)(uintptr_t)peb);
  put64(peb + PEB_IMAGE_BASE, (uint64_t)(uintptr_t)image);
  put64(peb + PEB_PROCESS_HEAP, (uint64_t)(uintptr_t)&process_heap);
  /* musl uses %fs for its own thread data; %gs is free for the TEB. */
  if (syscall(SYS_arch_prctl, ARCH_SET_GS, teb) != 0) {
    perror("peload: arch_prctl(ARCH_SET_GS)");
    exit(126);
  }

  win_argc = argc;
  win_wargv = calloc(argc + 1, sizeof(*win_wargv));
  size_t nenv = 0;
  while (environ[nenv])
    nenv++;
  win_wenvp = calloc(nenv + 1, sizeof(*win_wenvp));
  if (!win_wargv || !win_wenvp) {
    fprintf(stderr, "peload: out of memory\n");
    exit(126);
  }
  for (int i = 0; i < argc; i++)
    win_wargv[i] = to_utf16(argv[i]);
  for (size_t i = 0; i < nenv; i++)
    win_wenvp[i] = to_utf16(environ[i]);

  atexit(run_atexit);
}
