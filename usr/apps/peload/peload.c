/*
 * peload - run a Windows x64 console program (PE32+ .exe).
 *
 *   peload prog.exe [args...]
 *
 * The kernel runs .exe files through this automatically (exec.c), so
 * "./prog.exe" works too. Windows programs never make syscalls: they call
 * DLLs (kernel32.dll, ...). peload maps the .exe, applies its base
 * relocations, and fills its import table with the functions in win32.c,
 * which implement the Windows API on top of Linux syscalls.
 *
 * Supported: programs that import only what win32.c provides; peload
 * lists everything else that is missing before it gives up.
 *
 * Everything in the .exe is untrusted input: every offset and size is
 * checked against the file or image before use.
 */
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "peload.h"

/* -- PE format -------------------------------------------------------------
 * Field offsets from the PE/COFF specification. */

#define DOS_LFANEW 0x3c
#define COFF_SIZE 20
#define COFF_MACHINE 0
#define COFF_NSECTIONS 2
#define COFF_OPTSIZE 16
#define COFF_FLAGS 18
#define RELOCS_STRIPPED 0x0001
#define MACHINE_AMD64 0x8664

#define OPT_MAGIC 0
#define OPT_ENTRY 16
#define OPT_IMAGEBASE 24
#define OPT_SIZEOFIMAGE 56
#define OPT_SIZEOFHEADERS 60
#define OPT_NDIRS 108
#define OPT_DIRS 112
#define PE32PLUS 0x20b
#define DIR_IMPORT 1
#define DIR_RESOURCE 2
#define DIR_BASERELOC 5

#define SEC_SIZE 40
#define SEC_VSIZE 8
#define SEC_VADDR 12
#define SEC_RAWSIZE 16
#define SEC_RAWPTR 20

#define IMP_SIZE 20
#define IMP_LOOKUP 0 /* OriginalFirstThunk */
#define IMP_NAME 12
#define IMP_IAT 16   /* FirstThunk */

#define RES_DIR_SIZE 16
#define RES_NNAMED 12 /* entries named by string, listed first */
#define RES_NIDS 14
#define RES_SUBDIR 0x80000000u

#define DELAY_ATTRS 0
#define DELAY_NAME 4
#define DELAY_IAT 12
#define DELAY_INT 16
#define DELAY_RVA_BASED 1

#define REL_ABSOLUTE 0
#define REL_DIR64 10

const char *exe_path;
uint8_t *image;
uint32_t image_size;
static uint8_t *file;
static size_t file_size;
static const uint8_t *opt_header; /* in the image, once mapped */

static void die(const char *msg) {
  fprintf(stderr, "peload: %s: %s\n", exe_path, msg);
  exit(126);
}

/* [off, off + len) of the file, or die. */
static uint8_t *file_at(uint64_t off, uint64_t len) {
  if (off > file_size || len > file_size - off)
    die("truncated or corrupt file");
  return file + off;
}

/* [rva, rva + len) of the mapped image, or die. */
uint8_t *image_at(uint64_t rva, uint64_t len) {
  if (rva > image_size || len > image_size - rva)
    die("address outside the image");
  return image + rva;
}

/* NUL-terminated string at rva, which must end inside the image. */
static const char *image_str(uint32_t rva) {
  const char *s = (const char *)image_at(rva, 1);
  if (!memchr(s, 0, image_size - rva))
    die("unterminated name");
  return s;
}

static void read_file(const char *path) {
  int fd = open(path, O_RDONLY);
  struct stat st;
  if (fd < 0 || fstat(fd, &st) < 0) {
    perror(path);
    exit(127);
  }
  file_size = st.st_size;
  file = malloc(file_size ? file_size : 1);
  if (!file)
    die("out of memory");
  for (size_t got = 0; got < file_size;) {
    ssize_t n = read(fd, file + got, file_size - got);
    if (n <= 0)
      die("read failed");
    got += n;
  }
  close(fd);
}

/* Map the headers and every section at their RVAs. */
static void map_image(const uint8_t *opt, const uint8_t *sections,
                      unsigned nsections) {
  image_size = u32(opt + OPT_SIZEOFIMAGE);
  if (!image_size)
    die("empty image");
  /* Anywhere, not at ImageBase: relocations fix up the difference, and
   * this kernel's MAP_FIXED would replace whatever is mapped there. */
  image = mmap(NULL, image_size, PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (image == MAP_FAILED)
    die("cannot map image");

  uint32_t hdrs = u32(opt + OPT_SIZEOFHEADERS);
  memcpy(image_at(0, hdrs), file_at(0, hdrs), hdrs);

  for (unsigned i = 0; i < nsections; i++) {
    const uint8_t *s = sections + i * SEC_SIZE;
    uint32_t vsize = u32(s + SEC_VSIZE), vaddr = u32(s + SEC_VADDR);
    uint32_t raw = u32(s + SEC_RAWSIZE);
    /* Raw data is padded to the file alignment; VirtualSize is the real
     * size (0 in some linkers' output). The rest stays zero (.bss). */
    if (vsize && raw > vsize)
      raw = vsize;
    memcpy(image_at(vaddr, raw), file_at(u32(s + SEC_RAWPTR), raw), raw);
  }
}

/* Data directory @p which of the image: its RVA, and its size (0 if the
 * image has none). */
static uint32_t data_dir(unsigned which, uint32_t *size) {
  if (u32(opt_header + OPT_NDIRS) <= which) {
    *size = 0;
    return 0;
  }
  const uint8_t *dir = opt_header + OPT_DIRS + which * 8;
  *size = u32(dir + 4);
  return u32(dir);
}

/* Add (actual - preferred) base to every absolute address in the image. */
static void relocate(uint16_t coff_flags, uint64_t delta) {
  if (!delta)
    return;
  /* Only this flag means "must load at ImageBase". No relocation table
   * without it just means no absolute addresses (all RIP-relative). */
  if (coff_flags & RELOCS_STRIPPED)
    die("relocations stripped, and the image cannot be loaded at its base");
  uint32_t size, rva = data_dir(DIR_BASERELOC, &size);
  if (!size)
    return;

  const uint8_t *p = image_at(rva, size), *end = p + size;
  while (end - p >= 8) {
    uint32_t page = u32(p), block = u32(p + 4);
    if (block < 8 || block > (size_t)(end - p))
      die("corrupt relocation block");
    for (uint32_t off = 8; off + 2 <= block; off += 2) {
      uint16_t e = u16(p + off);
      if (e >> 12 == REL_ABSOLUTE)
        continue; /* padding */
      if (e >> 12 != REL_DIR64)
        die("unsupported relocation type");
      uint8_t *at = image_at((uint64_t)page + (e & 0xfff), 8);
      uint64_t v = u64(at) + delta;
      memcpy(at, &v, 8);
    }
    p += block;
  }
}

/* Point every imported function at our implementation. */
static void link_imports(void) {
  uint32_t size, rva = data_dir(DIR_IMPORT, &size);
  if (!size)
    return; /* imports nothing */

  int missing = 0;
  for (;; rva += IMP_SIZE) {
    const uint8_t *imp = image_at(rva, IMP_SIZE);
    uint32_t name = u32(imp + IMP_NAME), iat = u32(imp + IMP_IAT);
    if (!name && !iat)
      break; /* all-zero descriptor ends the list */
    const char *dll = image_str(name);
    /* The lookup table names the functions; the IAT gets the addresses.
     * Some linkers leave the lookup table out and name them in the IAT. */
    uint32_t lookup = u32(imp + IMP_LOOKUP) ? u32(imp + IMP_LOOKUP) : iat;

    for (uint64_t i = 0;; i++) {
      uint64_t entry = u64(image_at(lookup + i * 8, 8));
      if (!entry)
        break;
      if (entry >> 63) { /* by ordinal: there is no name to look up */
        fprintf(stderr, "peload: %s: %s!#%u: imports by ordinal are not supported\n",
                exe_path, dll, (unsigned)(entry & 0xffff));
        missing++;
        continue;
      }
      const char *fn = image_str((uint32_t)entry + 2); /* skip the hint */
      void *addr = find_import(dll, fn);
      if (!addr) {
        fprintf(stderr, "peload: %s: %s!%s is not implemented\n", exe_path,
                dll, fn);
        missing++;
        continue;
      }
      uint64_t a = (uint64_t)(uintptr_t)addr;
      memcpy(image_at(iat + i * 8, 8), &a, 8);
    }
  }
  if (missing)
    exit(127); /* report them all before giving up */
}

/* The entry for @p id in the resource directory at @p off (relative to
 * the resource section @p base), or 0 if there is none. */
static uint32_t res_lookup(uint32_t base, uint32_t off, uint32_t id) {
  const uint8_t *dir = image_at((uint64_t)base + off, RES_DIR_SIZE);
  unsigned named = u16(dir + RES_NNAMED), ids = u16(dir + RES_NIDS);
  const uint8_t *e =
      image_at((uint64_t)base + off + RES_DIR_SIZE, (named + ids) * 8ULL);
  for (unsigned i = named; i < named + ids; i++)
    if (u32(e + i * 8) == id)
      return u32(e + i * 8 + 4);
  return 0;
}

#define LANG_NEUTRAL 0
#define LANG_EN_US 0x409

/* Resources are a three-level tree: type, then id, then language. */
const uint8_t *find_resource(uint32_t type, uint32_t id, uint32_t *size) {
  uint32_t len, base = data_dir(DIR_RESOURCE, &len);
  if (!len)
    return NULL;
  uint32_t ids = res_lookup(base, 0, type);
  if (!(ids & RES_SUBDIR))
    return NULL;
  uint32_t langs = res_lookup(base, ids & ~RES_SUBDIR, id);
  if (!(langs & RES_SUBDIR))
    return NULL;
  langs &= ~RES_SUBDIR;

  /* We have no UI language setting: neutral, else US English, else
   * whichever language comes first. */
  uint32_t data = res_lookup(base, langs, LANG_NEUTRAL);
  if (!data)
    data = res_lookup(base, langs, LANG_EN_US);
  if (!data) {
    const uint8_t *dir = image_at((uint64_t)base + langs, RES_DIR_SIZE);
    if (!u16(dir + RES_NNAMED) && !u16(dir + RES_NIDS))
      return NULL;
    data = u32(image_at((uint64_t)base + langs + RES_DIR_SIZE + 4, 4));
  }
  if (data & RES_SUBDIR)
    die("corrupt resource directory");
  const uint8_t *entry = image_at((uint64_t)base + data, 8);
  *size = u32(entry + 4);
  return image_at(u32(entry), *size);
}

/* RVA of a pointer the program passed us, which must point into it. */
static uint32_t image_rva(const void *p, uint64_t len) {
  uint64_t rva = (uint64_t)((const uint8_t *)p - image);
  image_at(rva, len);
  return (uint32_t)rva;
}

/* A delay-loaded import is resolved on its first call, by the program's
 * own stub calling ResolveDelayLoadedAPI with the descriptor of its DLL
 * and the IAT slot to fill. */
void *resolve_delay_import(const void *desc_ptr, void *thunk) {
  const uint8_t *desc = image_at(image_rva(desc_ptr, 0), DELAY_INT + 4);
  if (!(u32(desc + DELAY_ATTRS) & DELAY_RVA_BASED))
    die("unsupported delay-load descriptor");
  const char *dll = image_str(u32(desc + DELAY_NAME));

  /* The slot's index in the IAT picks the name in the name table. Signed:
   * some linkers point both tables past their start, so it may be < 0. */
  int64_t index =
      ((int64_t)image_rva(thunk, 8) - (int64_t)u32(desc + DELAY_IAT)) / 8;
  int64_t name_rva = (int64_t)u32(desc + DELAY_INT) + index * 8;
  if (name_rva < 0)
    die("corrupt delay-load descriptor");
  uint64_t entry = u64(image_at((uint64_t)name_rva, 8));
  if (entry >> 63)
    die("delay-load imports by ordinal are not supported");
  const char *fn = image_str((uint32_t)entry + 2); /* skip the hint */

  void *addr = find_import(dll, fn);
  if (!addr) {
    fprintf(stderr, "peload: %s: %s!%s is not implemented\n", exe_path, dll,
            fn);
    exit(127);
  }
  memcpy(thunk, &addr, 8);
  return addr;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: peload prog.exe [args...]\n");
    return 2;
  }
  exe_path = argv[1];
  read_file(exe_path);

  if (u16(file_at(0, 2)) != 0x5a4d) /* "MZ" */
    die("not a Windows executable");
  uint32_t pe = u32(file_at(DOS_LFANEW, 4));
  if (memcmp(file_at(pe, 4), "PE\0\0", 4) != 0)
    die("no PE header");
  const uint8_t *coff = file_at(pe + 4, COFF_SIZE);
  if (u16(coff + COFF_MACHINE) != MACHINE_AMD64)
    die("not an x86_64 program");
  uint16_t optsize = u16(coff + COFF_OPTSIZE);
  const uint8_t *opt = file_at(pe + 4 + COFF_SIZE, optsize);
  if (optsize < OPT_DIRS || u16(opt + OPT_MAGIC) != PE32PLUS)
    die("not a PE32+ (64-bit) image");
  if (OPT_DIRS + (uint64_t)u32(opt + OPT_NDIRS) * 8 > optsize)
    die("corrupt optional header");
  unsigned nsections = u16(coff + COFF_NSECTIONS);
  uint16_t coff_flags = u16(coff + COFF_FLAGS);
  const uint8_t *sections =
      file_at(pe + 4 + COFF_SIZE + optsize, (uint64_t)nsections * SEC_SIZE);

  map_image(opt, sections, nsections);
  /* From here on read the headers from the image: the file is freed. */
  opt = opt_header = image_at(pe + 4 + COFF_SIZE, optsize);
  free(file);
  file = NULL;
  file_size = 0;

  relocate(coff_flags, (uint64_t)(uintptr_t)image - u64(opt + OPT_IMAGEBASE));
  link_imports();

  win32_init(argc - 1, argv + 1); /* the program's argv[0] is the .exe */

  uint32_t entry = u32(opt + OPT_ENTRY);
  int (WINAPI *start)(void) = (int (WINAPI *)(void))image_at(entry, 1);
  fflush(NULL); /* the program writes with WriteFile, not our stdio */
  return start(); /* a start routine that returns ends the process */
}
