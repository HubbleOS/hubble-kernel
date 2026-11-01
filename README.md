# Kernel HubbleOS

Kernel HubbleOS is a customizable operating system kernel with support for multiple architectures. 

## Building

You need:

- an `x86_64-elf` cross GCC and binutils, plus `nasm`
- Rust with the `x86_64-unknown-none` target (`rustup target add x86_64-unknown-none`)
- `limine` and `qemu-system-x86_64` to make an ISO and run it
- optional, for the userland: a static musl compiler (`x86_64-linux-musl-gcc`, from
  [musl.cc](https://musl.cc)) for the demos and the desktop, `x86_64-w64-mingw32-gcc` for the
  Windows test programs, and a busybox binary in `busy/`

Then:

```sh
make config   # creates config.mk and shows which tools were found
make run      # builds the kernel and the initramfs, boots it in QEMU
```

`config.mk` holds your local settings and is not committed: where your toolchains live,
which optional userland parts to build (each `y`, `n` or `auto`), and the QEMU accelerator,
memory and CPU count. `config.mk.example` documents every option. Any option can also be
given for one build on the command line, e.g. `make run QEMU_ACCEL=tcg CONFIG_GUI=n`.

`make usr` (and so `make run`) also builds `out/disk.img`, a test disk with a FAT32 and an
ext2 partition that QEMU attaches as the first IDE disk; the kernel mounts the ext2 one at
`/ext2`. Put test files in `usr/disk/`. It needs `mke2fs`, `mkfs.fat` and mtools, but not
root. `make disk` builds only the disk.

`make help` lists the other targets.
