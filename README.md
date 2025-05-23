# Kernel HubbleOS

## Overview

Kernel HubbleOS is a customizable operating system kernel with support for multiple architectures (x86 and ARM64). This repository contains the kernel source code, build tools, and instructions for running in the QEMU emulator.

> **Recommended**: Use Docker to build and run HubbleOS without installing toolchains manually.  
> For manual builds (Linux only), see [Required Technologies](#required-technologies--tools-without-docker).


## Quick Start

```bash
# Clone the repository into a folder named 'hubble-kernel'
git clone https://github.com/your-repo/hubbleos.git hubble-kernel

# Change directory into the cloned folder
cd hubble-kernel

make                # Build the kernel (default architecture is x86)
make run            # Run the kernel in QEMU
make img            # Build the kernel image
make flash          # Flash the kernel image to a USB device (e.g., /dev/sdX)
```

## Commands

```bash
make                # Build the kernel (default target)
make build          # Same as 'make', builds the kernel
make run            # Run the built kernel in QEMU
make img            # Build the kernel image 
make flash          # Flash the kernel image to a USB device
make clean          # Remove all build output
make help           # Show help message
```

## Usage

### You can optionally specify the architecture:

```bash
make [TARGET] ARCH=x86       # Build for x86 architecture (default)
make [TARGET] ARCH=arm64     # Build for ARM64 architecture
```

## Docker Build and Run

### 1. Choose your environment

Copy one of the example configs:

```bash
cp config/dev.env .env  # For Linux/macOS
```

### 2. Build the Docker images

```bash
docker-compose --env-file .env build
```

### 3. Run a container (interactive shell)

```bash
# With env file:
docker-compose --env-file .env build x86-builder
docker-compose --env-file .env build arm64-builder

# Or without:
docker-compose build x86-builder
docker-compose build arm64-builder

docker-compose run --rm x86-builder bash

```

### Run container with default command:

```bash
docker-compose run x86-builder
```

## Required Technologies & Tools (without Docker)

If you want to build and run the kernel **without using Docker**, make sure the following tools and dependencies are installed on your system.

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
