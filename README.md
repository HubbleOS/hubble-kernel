# Kernel HubbleOS

Kernel HubbleOS is a customizable operating system kernel with support for multiple architectures.

## Development Setup

### Prerequisites

- Cross-compiler toolchain (x86_64-elf-gcc, aarch64-elf-gcc)
- NASM (for x86 assembly)
- Limine bootloader (for QEMU testing)
- QEMU (for testing)

### Building

```bash
make build          # Build normal kernel (out/kernel.elf)
make debug-build    # Build debug kernel (out/debug/kernel.elf)
```

### Running in QEMU

```bash
make run            # Build and run kernel in QEMU
make debug          # Build debug kernel and run QEMU paused for GDB
```

### Debugging

**Terminal 1** — start QEMU paused with GDB stub:

```bash
make debug
```

**Terminal 2** — connect GDB:

```bash
make gdb
```

Then in GDB:

```
break kmain
continue
```

The debug build uses `-Og` optimization and DWARF-4 debug symbols for optimal debugging experience.

## Build Targets

| Target        | Description                                        |
| ------------- | -------------------------------------------------- |
| `build`       | Build normal kernel ELF                            |
| `debug-build` | Build debug kernel with DWARF symbols              |
| `run`         | Build and run kernel in QEMU                       |
| `debug`       | Build debug kernel and run QEMU paused for GDB     |
| `gdb`         | Connect GDB to running QEMU debug session          |
| `clean`       | Remove build artifacts                             |
| `rebuild`     | Clean and rebuild                                  |

## Architecture

The kernel uses the Limine boot protocol. For standalone development, the `make run` and `make debug` targets create a minimal Limine-based ISO automatically.

## Project Structure

```
hubble-kernel/
├── arch/           # Architecture-specific code (x86, arm64)
├── drivers/        # Device drivers
├── fs/             # Filesystem implementations
├── include/        # Header files
├── init/           # Kernel initialization
├── kernel/         # Core kernel subsystems
├── lib/            # Library code
├── net/            # Network stack
├── rust/           # Rust components
├── scripts/        # Build and QEMU scripts
├── sound/          # Audio subsystem
└── tools/          # Build tools
```

