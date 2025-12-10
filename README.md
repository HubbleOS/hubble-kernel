# Kernel HubbleOS

## Overview

Kernel HubbleOS is a customizable operating system kernel with support for multiple architectures (x86 and ARM64). This repository contains the kernel source code, build tools, and instructions for running in the QEMU emulator.


## Quick Start

### Clone the repository into a folder named 'hubble-kernel'
```bash
git clone --recurse-submodules https://github.com/HubbleOS/Kernel.git hubble && cd hubble-kernel
```
### Basic commands for work
```bash
make                # Build the kernel (default architecture is x86)
make run            # Run the kernel in QEMU
make img            # Build the kernel image
make flash          # Flash the kernel image to a USB device (e.g., /dev/sdX)
```

## Makefile Targets

| Command       | Description                                    |
| ------------- | ---------------------------------------------- |
| `make`        | Build the kernel (default target).             |
| `make build`  | Same as `make`, builds the kernel.             |
| `make run`    | Run the built kernel in QEMU.                  |
| `make qemu`   | Run QEMU using the current build output.       |
| `make img`    | Build the kernel image.                        |
| `make flash`  | Flash the kernel image to a USB device.        |
| `make clean`  | Remove all build output.                       |
| `make mkvars` | Print key build variables (debug info)         |
| `make help`   | Show the help message with available commands. |


## Usage

### You can optionally specify the architecture:

```bash
make [TARGET] ARCH=x86       # Build for x86 architecture (default)
make [TARGET] ARCH=arm64     # Build for ARM64 architecture
```

## Required Technologies & Tools


### Build dependencies

You need these packages to compile the kernel and toolchains:

- `build-essential` (gcc, make, etc.)  
- `bison`  
- `flex`  
- `libgmp3-dev`  
- `libmpc-dev`  
- `libmpfr-dev`  
- `texinfo`  
- `wget`  
- `git`  
- `gawk`  
- `libisl-dev`  
- `curl`  
- `ca-certificates`  
- `xz-utils`  
- `mtools`

## QEMU (emulator)

**QEMU** — to run the kernel in an emulator.  
  Official website: [https://www.qemu.org/](https://www.qemu.org/)  
