/* Linker script for Rust kernel modules (ld -r).
 *
 * The module loader maps every allocated section on its own page and
 * accepts at most 128 sections, while rustc emits one section per
 * function. Merge them into the handful the loader expects, and drop
 * unwind tables (.eh_frame is SHT_X86_64_UNWIND, which the loader
 * rejects; panic=abort never unwinds anyway). */
SECTIONS
{
  .text        : { *(.text .text.*) }
  .rodata      : { *(.rodata .rodata.*) }
  .data        : { *(.data .data.*) }
  .bss         : { *(.bss .bss.* COMMON) }
  .module.init : { KEEP(*(.module.init)) }
  .module.name : { KEEP(*(.module.name)) }

  /DISCARD/ : {
    *(.eh_frame*) *(.comment) *(.note .note.*)
    *(.llvmbc) *(.llvmcmd)   /* embedded LLVM bitcode, megabytes, unused */
  }
}
