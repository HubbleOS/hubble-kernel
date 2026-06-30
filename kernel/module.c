#include <hubble/module.h>
#include <hubble/elf.h>

#include <hubble/printk.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/init.h>
#include <hubble/device.h>
#include <hubble/ctype.h>

#include <fs/vfs/vfs.h>

#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#include <higher_half.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <interrupt/interrupt.h>
#include <drivers/pci/pci.h>
#include <fs/vfs/dev.h>
#include <hubble/platform.h>


/* ── helpers ─────────────────────────────────────────────────────────────── */

static int local_strcasecmp(const char *s1, const char *s2)
{
	while (*s1 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2)))
	{
		s1++;
		s2++;
	}
	return (int)(tolower((unsigned char)*s1) - tolower((unsigned char)*s2));
}

/* ── externs for symbols needed by modules ────────────────────────────────── */
extern void early_putchar(void);
extern void hpet_delay_ms(void);
extern void waitqueue_init(void);
extern void waitqueue_sleep(void);
extern void waitqueue_wake_all(void);

/* ── kernel exported symbol table ────────────────────────────────────────── */

typedef struct
{
	const char *name;
	uint64_t addr;
} module_export_t;

static const module_export_t g_exports[] = {
	{"printk", (uint64_t)(uintptr_t)printk},
	{"kmalloc", (uint64_t)(uintptr_t)kmalloc},
	{"kfree", (uint64_t)(uintptr_t)kfree},
	{"kzalloc", (uint64_t)(uintptr_t)kzalloc},
	{"krealloc", (uint64_t)(uintptr_t)krealloc},
	{"kcalloc", (uint64_t)(uintptr_t)kcalloc},
	{"ksize", (uint64_t)(uintptr_t)ksize},
	{"vfs_open", (uint64_t)(uintptr_t)vfs_open},
	{"vfs_read", (uint64_t)(uintptr_t)vfs_read},
	{"vfs_lseek", (uint64_t)(uintptr_t)vfs_lseek},
	{"vfs_close", (uint64_t)(uintptr_t)vfs_close},
	{"vfs_readdir", (uint64_t)(uintptr_t)vfs_readdir},
	{"device_register", (uint64_t)(uintptr_t)device_register},
	{"device_find_by_name", (uint64_t)(uintptr_t)device_find_by_name},
	{"device_find_by_type", (uint64_t)(uintptr_t)device_find_by_type},
	{"pmm_alloc_page", (uint64_t)(uintptr_t)pmm_alloc_page},
	{"pmm_alloc_pages", (uint64_t)(uintptr_t)pmm_alloc_pages},
	{"pmm_free_page", (uint64_t)(uintptr_t)pmm_free_page},
	{"pmm_free_pages", (uint64_t)(uintptr_t)pmm_free_pages},
	{"vmm_map_page", (uint64_t)(uintptr_t)vmm_map_page},
	{"vmm_unmap_page", (uint64_t)(uintptr_t)vmm_unmap_page},
	{"vmm_set_flags", (uint64_t)(uintptr_t)vmm_set_flags},
	{"vmm_get_phys", (uint64_t)(uintptr_t)vmm_get_phys},
	{"vmm_is_mapped", (uint64_t)(uintptr_t)vmm_is_mapped},
	{"vmm_unmap_user_page", (uint64_t)(uintptr_t)vmm_unmap_user_page},
	{"vmm_map_page_into", (uint64_t)(uintptr_t)vmm_map_page_into},
	{"vmm_get_phys_from", (uint64_t)(uintptr_t)vmm_get_phys_from},
	{"vmm_create_user_pagemap", (uint64_t)(uintptr_t)vmm_create_user_pagemap},
	{"memcpy", (uint64_t)(uintptr_t)memcpy},
	{"memset", (uint64_t)(uintptr_t)memset},
	{"memmove", (uint64_t)(uintptr_t)memmove},
	{"memcmp", (uint64_t)(uintptr_t)memcmp},
	{"memchr", (uint64_t)(uintptr_t)memchr},
	{"strlen", (uint64_t)(uintptr_t)strlen},
	{"strcpy", (uint64_t)(uintptr_t)strcpy},
	{"strncpy", (uint64_t)(uintptr_t)strncpy},
	{"strcat", (uint64_t)(uintptr_t)strcat},
	{"strncat", (uint64_t)(uintptr_t)strncat},
	{"strcmp", (uint64_t)(uintptr_t)strcmp},
	{"strncmp", (uint64_t)(uintptr_t)strncmp},
	{"strchr", (uint64_t)(uintptr_t)strchr},
	{"strrchr", (uint64_t)(uintptr_t)strrchr},
	{"pci_register_driver", (uint64_t)(uintptr_t)pci_register_driver},
	{"irq_install_handler", (uint64_t)(uintptr_t)irq_install_handler},
	{"pci_read_config", (uint64_t)(uintptr_t)pci_read_config},
	{"pci_write_config", (uint64_t)(uintptr_t)pci_write_config},
	{"pci_set_command", (uint64_t)(uintptr_t)pci_set_command},
	{"dev_vfs_register", (uint64_t)(uintptr_t)dev_vfs_register},
	{"early_putchar", (uint64_t)(uintptr_t)early_putchar},
	{"hpet_delay_ms", (uint64_t)(uintptr_t)hpet_delay_ms},
	{"waitqueue_init", (uint64_t)(uintptr_t)waitqueue_init},
	{"waitqueue_sleep", (uint64_t)(uintptr_t)waitqueue_sleep},
	{"waitqueue_wake_all", (uint64_t)(uintptr_t)waitqueue_wake_all},
	{"g_platform", (uint64_t)(uintptr_t)&g_platform},
	{"isalpha", (uint64_t)(uintptr_t)isalpha},
	{"tolower", (uint64_t)(uintptr_t)tolower},
	{"toupper", (uint64_t)(uintptr_t)toupper},
};

#define EXPORT_COUNT (sizeof(g_exports) / sizeof(g_exports[0]))

/* ── section tracking for loading ────────────────────────────────────────── */

typedef struct
{
	const char *name;
	uint64_t base;
	uint64_t size;
	uint64_t flags;
	uint32_t sh_type;
	uint64_t sh_addralign;
} module_section_t;

#define MODULE_VIRT_BASE (KERNEL_VIRT_BASE + 0x10000000ULL)
#define MODULE_VIRT_LIMIT (MODULE_VIRT_BASE + 0x10000000ULL)

static uint64_t g_module_next_virt = MODULE_VIRT_BASE;

/* ── per-module symbol export ────────────────────────────────────────────── */

#define MODULE_MAX_EXPORTS 512

typedef struct
{
	char name[64];
	uint64_t addr;
} module_sym_export_t;

/* ── module data structures ────────────────────────────────────────────────── */

typedef struct module_section_info
{
	char name[64];
	uint64_t base;
	uint64_t size;
} module_section_info_t;

#define MODULE_MAX_SECTIONS 128

struct module
{
	char name[64];
	char path[256];
	uint64_t base;
	size_t size;
	void (*exit_fn)(void);
	struct module *next;
	module_section_info_t sections[MODULE_MAX_SECTIONS];
	int section_count;
	int export_count;
	module_sym_export_t exports[MODULE_MAX_EXPORTS];
};

static module_t *g_modules_list = NULL;

/* ── symbol resolution (with loaded-module fallback) ──────────────────────── */

static uint64_t module_resolve_kernel_export(const char *name)
{
	for (size_t i = 0; i < EXPORT_COUNT; i++)
	{
		if (strcmp(g_exports[i].name, name) == 0)
			return g_exports[i].addr;
	}
	return 0;
}

static uint64_t module_resolve_module_export(const char *name)
{
	module_t *cur = g_modules_list;
	while (cur)
	{
		for (int i = 0; i < cur->export_count; i++)
		{
			if (strcmp(cur->exports[i].name, name) == 0)
				return cur->exports[i].addr;
		}
		cur = cur->next;
	}
	return 0;
}

static uint64_t module_resolve_export(const char *name)
{
	uint64_t addr = module_resolve_kernel_export(name);
	if (addr)
		return addr;
	return module_resolve_module_export(name);
}

/* ── helpers ─────────────────────────────────────────────────────────────── */

static size_t module_align_up(size_t value, size_t align)
{
	if (align == 0)
		align = 1;
	return (value + align - 1) & ~(align - 1);
}

static bool module_name_ends_with(const char *name, const char *suffix)
{
	size_t name_len = strlen(name);
	size_t suffix_len = strlen(suffix);
	if (name_len < suffix_len)
		return false;
	return local_strcasecmp(name + (name_len - suffix_len), suffix) == 0;
}

static const char *module_section_name(const char *shstrtab, const Elf64_Shdr *shdr)
{
	return shstrtab + shdr->sh_name;
}

static bool section_name_eq(const char *name, const char *prefix)
{
	return strncmp(name, prefix, strlen(prefix)) == 0;
}

/* ── symbol resolution for relocations ────────────────────────────────────── */

static uint64_t module_resolve_symbol(const Elf64_Sym *sym,
				      const module_section_t *sections,
				      size_t section_count,
				      const char *strtab,
				      size_t strtab_size)
{
	if (sym->st_shndx == SHN_UNDEF)
	{
		if (sym->st_name >= strtab_size)
			return 0;
		return module_resolve_export(strtab + sym->st_name);
	}

	if (sym->st_shndx == SHN_ABS)
		return sym->st_value;

	if (sym->st_shndx == SHN_COMMON)
		return 0;

	if (sym->st_shndx >= section_count)
		return 0;

	if (!sections[sym->st_shndx].base)
		return 0;

	return sections[sym->st_shndx].base + sym->st_value;
}

/* ── relocation ──────────────────────────────────────────────────────────── */

static int module_apply_relocation(uint64_t target_base,
				   size_t target_size,
				   const Elf64_Rela *rela,
				   const Elf64_Sym *syms,
				   size_t sym_count,
				   const char *strtab,
				   size_t strtab_size,
				   const module_section_t *sections,
				   size_t section_count)
{
	uint32_t type = ELF64_R_TYPE(rela->r_info);
	uint32_t sym_index = ELF64_R_SYM(rela->r_info);
	size_t patch_size;

	if (sym_index >= sym_count)
		return -ENOEXEC;

	uint64_t symbol = module_resolve_symbol(&syms[sym_index], sections, section_count,
						strtab, strtab_size);
	switch (type)
	{
	case R_X86_64_64:
		patch_size = 8;
		break;
	case R_X86_64_PC32:
	case R_X86_64_PLT32:
	case R_X86_64_32:
	case R_X86_64_32S:
		patch_size = 4;
		break;
	case R_X86_64_16:
		patch_size = 2;
		break;
	case R_X86_64_8:
		patch_size = 1;
		break;
	default:
		patch_size = 0;
		break;
	}

	if (patch_size == 0)
	{
		if (type == R_X86_64_NONE)
			return 0;
		printk(KERN_ERR "[module] unsupported relocation type %u\n", type);
		return -ENOEXEC;
	}

	if (rela->r_offset > target_size || target_size - rela->r_offset < patch_size)
		return -ENOEXEC;

	uint64_t location = target_base + rela->r_offset;
	uint8_t *where = (uint8_t *)(uintptr_t)location;
	uint64_t value;

	switch (type)
	{
	case R_X86_64_NONE:
		return 0;
	case R_X86_64_64:
		value = symbol + rela->r_addend;
		memcpy(where, &value, sizeof(value));
		return 0;
	case R_X86_64_PC32:
	case R_X86_64_PLT32:
	{
		int64_t reloc = (int64_t)symbol + rela->r_addend - (int64_t)(uintptr_t)where;
		int32_t out = (int32_t)reloc;
		memcpy(where, &out, sizeof(out));
		return 0;
	}
	case R_X86_64_32:
	{
		uint32_t out = (uint32_t)(symbol + rela->r_addend);
		memcpy(where, &out, sizeof(out));
		return 0;
	}
	case R_X86_64_32S:
	{
		int32_t out = (int32_t)(symbol + rela->r_addend);
		memcpy(where, &out, sizeof(out));
		return 0;
	}
	case R_X86_64_16:
	{
		uint16_t out = (uint16_t)(symbol + rela->r_addend);
		memcpy(where, &out, sizeof(out));
		return 0;
	}
	case R_X86_64_8:
	{
		uint8_t out = (uint8_t)(symbol + rela->r_addend);
		memcpy(where, &out, sizeof(out));
		return 0;
	}
	default:
		return -ENOEXEC;
	}
}

/* ── memory mapping ──────────────────────────────────────────────────────── */

static uint64_t module_map_region(size_t size, uint64_t sh_flags, uint64_t sh_addralign)
{
	bool executable = (sh_flags & SHF_EXECINSTR) != 0;
	bool writable = (sh_flags & SHF_WRITE) != 0;
	size_t align = PAGE_SIZE;
	if (sh_addralign > align)
		align = (size_t)sh_addralign;
	size = module_align_up(size, PAGE_SIZE);

	uint64_t base = module_align_up(g_module_next_virt, align);
	uint64_t end = base + size;
	if (end < base || end > MODULE_VIRT_LIMIT)
		return 0;

	uint64_t mapped = 0;
	for (uint64_t va = base; va < end; va += PAGE_SIZE)
	{
		uint64_t phys = pmm_alloc_page();
		if (!phys)
			goto fail;

		uint64_t flags = PTE_PRESENT;
		if (writable)
			flags |= PTE_WRITE;
		if (!executable)
			flags |= PTE_NX;

		if (vmm_map_page(va, phys, flags) < 0)
		{
			pmm_free_page(phys);
			goto fail;
		}

		mapped += PAGE_SIZE;
	}

	g_module_next_virt = end;
	memset((void *)base, 0, size);
	return base;

fail:
	for (uint64_t va = base; va < base + mapped; va += PAGE_SIZE)
	{
		uint64_t phys = vmm_get_phys(va);
		if (phys)
		{
			vmm_unmap_page(va);
			pmm_free_page(phys);
		}
	}
	return 0;
}

void module_unmap_region(uint64_t base, size_t size)
{
	size = module_align_up(size, PAGE_SIZE);
	for (uint64_t va = base; va < base + size; va += PAGE_SIZE)
	{
		uint64_t phys = vmm_get_phys(va);
		if (phys)
		{
			vmm_unmap_page(va);
			pmm_free_page(phys);
		}
	}
}

/* ── module registry ─────────────────────────────────────────────────────── */

module_t *module_find(const char *name)
{
	module_t *curr = g_modules_list;
	while (curr)
	{
		if (local_strcasecmp(curr->name, name) == 0)
			return curr;
		curr = curr->next;
	}
	return NULL;
}

bool module_is_loaded(const char *name)
{
	return module_find(name) != NULL;
}

/* ── module loader ───────────────────────────────────────────────────────── */

int module_load_buffer(const void *image, size_t image_size)
{
	if (image_size < sizeof(Elf64_Ehdr))
		return -ENOEXEC;

	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)image;
	if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
	    ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3)
		return -ENOEXEC;

	if (ehdr->e_type != ET_REL || ehdr->e_machine != EM_X86_64 || ehdr->e_version != EV_CURRENT)
		return -ENOEXEC;

	if (ehdr->e_shoff == 0 || ehdr->e_shentsize != sizeof(Elf64_Shdr) || ehdr->e_shnum == 0)
		return -ENOEXEC;

	uint64_t sh_end = ehdr->e_shoff + (uint64_t)ehdr->e_shnum * sizeof(Elf64_Shdr);
	if (sh_end > image_size)
		return -ENOEXEC;

	Elf64_Shdr *shdrs = (Elf64_Shdr *)((uint8_t *)image + ehdr->e_shoff);
	if (ehdr->e_shstrndx >= ehdr->e_shnum)
		return -ENOEXEC;

	Elf64_Shdr *shstr_shdr = &shdrs[ehdr->e_shstrndx];
	if (shstr_shdr->sh_offset + shstr_shdr->sh_size > image_size)
		return -ENOEXEC;

	const char *shstrtab = (const char *)((uint8_t *)image + shstr_shdr->sh_offset);

	size_t section_count = ehdr->e_shnum;
	module_section_t *sections = kzalloc(section_count * sizeof(module_section_t));
	if (!sections)
		return -ENOMEM;

	const Elf64_Sym *symtab = NULL;
	size_t sym_count = 0;
	const char *strtab = NULL;
	size_t strtab_size = 0;

	void (*mod_exit_fn)(void) = NULL;

	for (size_t i = 0; i < section_count; i++)
	{
		Elf64_Shdr *shdr = &shdrs[i];
		const char *sec_name = module_section_name(shstrtab, shdr);

		if (shdr->sh_type == SHT_SYMTAB)
		{
			if (shdr->sh_offset + shdr->sh_size > image_size || shdr->sh_entsize == 0)
			{
				kfree(sections);
				return -ENOEXEC;
			}
			symtab = (const Elf64_Sym *)((uint8_t *)image + shdr->sh_offset);
			sym_count = shdr->sh_size / shdr->sh_entsize;
			if (shdr->sh_link >= section_count)
			{
				kfree(sections);
				return -ENOEXEC;
			}
			Elf64_Shdr *linked = &shdrs[shdr->sh_link];
			if (linked->sh_offset + linked->sh_size > image_size)
			{
				kfree(sections);
				return -ENOEXEC;
			}
			strtab = (const char *)((uint8_t *)image + linked->sh_offset);
			strtab_size = linked->sh_size;
			continue;
		}

		if (!(shdr->sh_flags & SHF_ALLOC) || shdr->sh_size == 0)
			continue;

		uint64_t base = module_map_region(shdr->sh_size, shdr->sh_flags, shdr->sh_addralign);
		if (!base)
		{
			kfree(sections);
			return -ENOMEM;
		}

		sections[i].name = sec_name;
		sections[i].base = base;
		sections[i].size = shdr->sh_size;
		sections[i].flags = shdr->sh_flags;
		sections[i].sh_type = shdr->sh_type;
		sections[i].sh_addralign = shdr->sh_addralign;

		if (shdr->sh_type == SHT_PROGBITS)
		{
			if (shdr->sh_offset + shdr->sh_size > image_size)
			{
				kfree(sections);
				return -ENOEXEC;
			}
			memcpy((void *)base, (uint8_t *)image + shdr->sh_offset, shdr->sh_size);
		}
		else if (shdr->sh_type == SHT_NOBITS)
		{
			memset((void *)base, 0, shdr->sh_size);
		}
		else
		{
			printk(KERN_ERR "[module] unsupported alloc section type %u (%s)\n",
			       shdr->sh_type, sec_name);
			kfree(sections);
			return -ENOEXEC;
		}
	}

	if (!symtab || !strtab)
	{
		kfree(sections);
		return -ENOEXEC;
	}

	if (strtab_size == 0)
	{
		kfree(sections);
		return -ENOEXEC;
	}

	/* Apply relocations */
	for (size_t i = 0; i < section_count; i++)
	{
		Elf64_Shdr *shdr = &shdrs[i];
		if (shdr->sh_type != SHT_RELA)
			continue;

		if (shdr->sh_info >= section_count || !sections[shdr->sh_info].base)
			continue;

		if (shdr->sh_entsize != sizeof(Elf64_Rela) ||
		    shdr->sh_offset + shdr->sh_size > image_size)
		{
			kfree(sections);
			return -ENOEXEC;
		}

		const Elf64_Rela *relas = (const Elf64_Rela *)((uint8_t *)image + shdr->sh_offset);
		size_t rela_count = shdr->sh_size / sizeof(Elf64_Rela);
		uint64_t target_base = sections[shdr->sh_info].base;
		for (size_t j = 0; j < rela_count; j++)
		{
			int ret = module_apply_relocation(target_base,
							  sections[shdr->sh_info].size,
							  &relas[j], symtab, sym_count, strtab,
							  strtab_size, sections, section_count);
			if (ret < 0)
			{
				kfree(sections);
				return ret;
			}
		}
	}

	/* Scan for module metadata */
	uint64_t mod_base = 0;
	size_t mod_size = 0;

	/* Collect exported symbols from the symtab for this module */
	module_sym_export_t mod_exports[MODULE_MAX_EXPORTS];
	int mod_export_count = 0;

	for (size_t i = 1; i < sym_count && mod_export_count < MODULE_MAX_EXPORTS; i++)
	{
		const Elf64_Sym *sym = &symtab[i];
		if (sym->st_name >= strtab_size)
			continue;

		uint8_t bind = ELF64_ST_BIND(sym->st_info);

		/* Export global defined symbols (not undefined) */
		if (bind != STB_GLOBAL)
			continue;

		if (sym->st_shndx == SHN_UNDEF)
			continue;

		const char *sym_name = strtab + sym->st_name;

		uint64_t sym_addr;
		if (sym->st_shndx == SHN_ABS)
			sym_addr = sym->st_value;
		else if (sym->st_shndx < section_count && sections[sym->st_shndx].base)
			sym_addr = sections[sym->st_shndx].base + sym->st_value;
		else
			continue;

		strncpy(mod_exports[mod_export_count].name, sym_name,
			sizeof(mod_exports[mod_export_count].name) - 1);
		mod_exports[mod_export_count].addr = sym_addr;
		mod_export_count++;
	}

	/* Scan for .module.exit section */
	for (size_t i = 0; i < section_count; i++)
	{
		if (!sections[i].base || !sections[i].name)
			continue;

		if (section_name_eq(sections[i].name, ".module.exit"))
		{
			if (sections[i].size >= sizeof(void (*)(void)))
				mod_exit_fn = *(void (**)(void))sections[i].base;
			continue;
		}

		if (section_name_eq(sections[i].name, ".text") ||
		    section_name_eq(sections[i].name, ".data") ||
		    section_name_eq(sections[i].name, ".rodata") ||
		    section_name_eq(sections[i].name, ".bss"))
		{
			if (mod_base == 0 || sections[i].base < mod_base)
				mod_base = sections[i].base;
			uint64_t end = sections[i].base + sections[i].size;
			if (end > mod_size)
				mod_size = end - mod_base;
		}
	}

	/* Register module before running init so its exports are available */
	module_t *mod = kzalloc(sizeof(module_t));
	if (!mod)
	{
		kfree(sections);
		return -ENOMEM;
	}

	strncpy(mod->name, "anonymous", sizeof(mod->name) - 1);
	mod->base = mod_base;
	mod->size = mod_size;
	mod->exit_fn = mod_exit_fn;
	memcpy(mod->exports, mod_exports, sizeof(module_sym_export_t) * mod_export_count);
	mod->export_count = mod_export_count;
	mod->next = g_modules_list;
	g_modules_list = mod;

	/* Run module.init sections */
	for (size_t i = 0; i < section_count; i++)
	{
		if (!sections[i].base || !sections[i].name)
			continue;
		if (!section_name_eq(sections[i].name, ".module.init"))
			continue;

		size_t count = sections[i].size / sizeof(initcall_t);
		initcall_t *calls = (initcall_t *)sections[i].base;
		for (size_t j = 0; j < count; j++)
		{
			if (!calls[j])
				continue;
			int ret = calls[j]();
			if (ret != 0)
			{
				printk(KERN_ERR "[module] module_init failed: %d\n", ret);
				g_modules_list = mod->next;
				module_unmap_region(mod_base, mod_size);
				kfree(mod);
				kfree(sections);
				return ret;
			}
		}

		goto load_done;
	}

	/* Fallback: look for initcall sections */
	for (size_t i = 0; i < section_count; i++)
	{
		if (!sections[i].base || !sections[i].name)
			continue;
		if (strncmp(sections[i].name, ".initcalls.", 11) != 0)
			continue;

		size_t count = sections[i].size / sizeof(initcall_t);
		initcall_t *calls = (initcall_t *)sections[i].base;
		for (size_t j = 0; j < count; j++)
		{
			if (!calls[j])
				continue;
			int ret = calls[j]();
			if (ret != 0)
			{
				printk(KERN_ERR "[module] initcall failed: %d\n", ret);
				g_modules_list = mod->next;
				module_unmap_region(mod_base, mod_size);
				kfree(mod);
				kfree(sections);
				return ret;
			}
		}

		goto load_done;
	}

	/* Fallback: look for module_init symbol */
	for (size_t i = 0; i < sym_count; i++)
	{
		if (symtab[i].st_name >= strtab_size)
			continue;
		const char *sym_name = strtab + symtab[i].st_name;
		if (strcmp(sym_name, "module_init") != 0)
			continue;

		uint64_t entry = module_resolve_symbol(&symtab[i], sections,
						      section_count, strtab, strtab_size);
		if (entry == 0)
			break;

		int (*module_init_fn)(void) = (int (*)(void))(uintptr_t)entry;
		int ret = module_init_fn();
		if (ret != 0)
		{
			printk(KERN_ERR "[module] module_init symbol failed: %d\n", ret);
			g_modules_list = mod->next;
			module_unmap_region(mod_base, mod_size);
			kfree(mod);
			kfree(sections);
			return ret;
		}
		break;
	}

load_done:
	kfree(sections);
	return 0;
}

/* ── update module name and metadata after loading ───────────────────────── */

static void module_set_name(const char *name)
{
	/* The most recently loaded module is at the head of the list */
	if (!g_modules_list)
		return;

	strncpy(g_modules_list->name, name, sizeof(g_modules_list->name) - 1);
	strncpy(g_modules_list->path, name, sizeof(g_modules_list->path) - 1);
}

/* ── public API ──────────────────────────────────────────────────────────── */

int module_load(const char *path)
{
	if (!path)
		return -EINVAL;

	VFS_File *file = vfs_open(path, VFS_O_RDONLY);
	if (IS_ERR(file))
		return PTR_ERR(file);
	if (!file)
		return -ENOENT;

	size_t size = file->node ? file->node->size : 0;
	if (size == 0)
	{
		vfs_close(file);
		return -ENOEXEC;
	}

	uint8_t *image = kmalloc(size, GFP_KERNEL);
	if (!image)
	{
		vfs_close(file);
		return -ENOMEM;
	}

	if (vfs_lseek(file, 0, SEEK_SET) < 0 || vfs_read(file, image, size) != (int)size)
	{
		kfree(image);
		vfs_close(file);
		return -EIO;
	}

	vfs_close(file);
	int ret = module_load_buffer(image, size);

	if (ret == 0)
	{
		const char *filename = strrchr(path, '/');
		if (filename)
			filename++;
		else
			filename = path;

		char mod_name[64];
		strncpy(mod_name, filename, sizeof(mod_name) - 1);
		char *dot = strrchr(mod_name, '.');
		if (dot && local_strcasecmp(dot, ".ko") == 0)
			*dot = '\0';

		module_set_name(mod_name);
		printk(KERN_OK "[module] loaded %s\n", path);
	}
	else
	{
		const char *filename = strrchr(path, '/');
		if (filename) filename++; else filename = path;

		if (strncmp(filename, "._", 2) != 0 && filename[0] != '.')
			printk(KERN_ERR "[module] failed to load %s: %d\n", path, ret);
	}

	kfree(image);
	return ret;
}

int module_load_directory(const char *path)
{
	if (!path)
		return -EINVAL;

	Directory dir = vfs_readdir(path);
	if (!dir.entries || dir.count <= 0)
		return -ENOENT;

	int loaded = 0;
	for (int i = 0; i < dir.count; i++)
	{
		Entry *entry = &dir.entries[i];
		if (!entry->name || entry->is_dir)
			continue;

		if (entry->name[0] == '.' || (entry->name[0] == '_' && strchr(entry->name, '~')))
			continue;

		if (!module_name_ends_with(entry->name, ".ko"))
			continue;

		size_t path_len = strlen(path);
		size_t name_len = strlen(entry->name);
		bool needs_slash = path_len > 0 && path[path_len - 1] != '/';
		size_t full_len = path_len + (needs_slash ? 1 : 0) + name_len + 1;
		char *full_path = kmalloc(full_len, GFP_KERNEL);
		if (!full_path)
			continue;

		strcpy(full_path, path);
		if (needs_slash)
			strcat(full_path, "/");
		strcat(full_path, entry->name);

		if (module_load(full_path) == 0)
			loaded++;

		kfree(full_path);
	}

	if (dir.free_entries)
		dir.free_entries(&dir);

	return loaded > 0 ? loaded : -ENOENT;
}

int module_unload(const char *name)
{
	if (!name)
		return -EINVAL;

	module_t *prev = NULL;
	module_t *curr = g_modules_list;

	while (curr)
	{
		if (local_strcasecmp(curr->name, name) == 0)
		{
			if (curr->exit_fn)
				curr->exit_fn();

			if (prev)
				prev->next = curr->next;
			else
				g_modules_list = curr->next;

			module_unmap_region(curr->base, curr->size);
			kfree(curr);
			printk(KERN_OK "[module] unloaded %s\n", name);
			return 0;
		}
		prev = curr;
		curr = curr->next;
	}

	return -ENOENT;
}

int module_refresh_symbols(void)
{
	return 0;
}
