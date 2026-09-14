#pragma once

/**
 * @brief ELF64 constants and structures for kernel use.
 *
 * Provides the definitions needed to parse ELF relocatable (.ko) files
 * and executable images.
 */

#include <stdint.h>

/* -- ELF identification ---------------------------------------------------- */

#define EI_NIDENT 16
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/* -- Object file types ------------------------------------------------------
 */

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3

#define EM_X86_64 62
#define EV_CURRENT 1

/* -- Section indices --------------------------------------------------------
 */

#define SHN_UNDEF 0
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2

/* -- Section types ----------------------------------------------------------
 */

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_NOBITS 8
#define SHT_NOTE 7

/* -- Section flags ----------------------------------------------------------
 */

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

/* -- Symbol binding ---------------------------------------------------------
 */

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

/* -- Symbol types -----------------------------------------------------------
 */

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4

/* -- Symbol/relocation accessors --------------------------------------------
 */

#define ELF64_ST_BIND(val) ((uint8_t)((val) >> 4))
#define ELF64_ST_TYPE(val) ((uint8_t)((val) & 0x0f))
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0x0f))

#define ELF64_R_SYM(info) ((uint32_t)((info) >> 32))
#define ELF64_R_TYPE(info) ((uint32_t)(info))
#define ELF64_R_INFO(s, t) ((((uint64_t)(s)) << 32) + (uint32_t)(t))

/* -- x86-64 relocation types ------------------------------------------------
 */

#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_GOT32 3
#define R_X86_64_PLT32 4
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_16 12
#define R_X86_64_8 14

/* -- ELF64 structures -------------------------------------------------------
 */

typedef struct {
  unsigned char e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
  uint32_t sh_name;
  uint32_t sh_type;
  uint64_t sh_flags;
  uint64_t sh_addr;
  uint64_t sh_offset;
  uint64_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint64_t sh_addralign;
  uint64_t sh_entsize;
} __attribute__((packed)) Elf64_Shdr;

typedef struct {
  uint32_t st_name;
  unsigned char st_info;
  unsigned char st_other;
  uint16_t st_shndx;
  uint64_t st_value;
  uint64_t st_size;
} __attribute__((packed)) Elf64_Sym;

typedef struct {
  uint64_t r_offset;
  uint64_t r_info;
  int64_t r_addend;
} __attribute__((packed)) Elf64_Rela;
