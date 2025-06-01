#!/bin/bash
set -e

WORKDIR="$(pwd)"

# Read debug port from config file or use default
CONFIG_FILE="$WORKDIR/.config"
DEFAULT_PORT=1234

if [ -f "$CONFIG_FILE" ]; then
	PORT=$(grep -E '^DEBUG_PORT=' "$CONFIG_FILE" | cut -d '=' -f 2)
fi

if [ -z "$PORT" ]; then
	PORT=$DEFAULT_PORT
fi

echo "Using debug port: $PORT"

# Check if ISO directory is provided and exists
if [ -z "$1" ]; then
	echo "❌ Missing ISO directory"
	exit 1
elif [ ! -d "$1" ]; then
	echo "❌ Directory not found: $1"
	exit 1
else
	ISO_DIR="$1"
fi

# OS specific handling for ISO path
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
	WIN_ISO_DIR=$(cygpath -w "$ISO_DIR")
	QEMU_ISO_PATH="$WIN_ISO_DIR"
	;;
Linux | Darwin)
	QEMU_ISO_PATH="$ISO_DIR"
	;;
*)
	echo "❌ Unsupported OS: $(uname -s)"
	exit 1
	;;
esac

# Run QEMU with OVMF
qemu-system-x86_64 \
	-M pc \
	-cpu Haswell \
	-smp 2 \
	-m 1024 \
	-drive if=pflash,format=raw,readonly=on,file=$WORKDIR/ovmf/OVMF_CODE.fd \
	-hda fat:rw:"$QEMU_ISO_PATH" \
	-serial stdio \
	-S -gdb tcp::${PORT}

# #!/bin/bash
# set -e

# WORKDIR="$(pwd)"

# if [ -z "$1" ]; then
# 	echo "❌ Missing ISO directory"
# 	exit 1
# elif [ ! -d "$1" ]; then
# 	echo "❌ Directory not found: $1"
# 	exit 1
# else
# 	ISO_DIR="$1"
# fi

# case "$(uname -s)" in
# MINGW* | MSYS* | CYGWIN*)
# 	WIN_ISO_DIR=$(cygpath -w "$ISO_DIR")
# 	QEMU_ISO_PATH="$WIN_ISO_DIR"
# 	;;
# Linux | Darwin)
# 	QEMU_ISO_PATH="$ISO_DIR"
# 	;;
# *)
# 	echo "❌ Unsupported OS: $(uname -s)"
# 	exit 1
# 	;;
# esac

# qemu-system-x86_64 \
# 	-M pc \
# 	-cpu Haswell \
# 	-smp 2 \
# 	-m 1024 \
# 	-drive if=pflash,format=raw,readonly=on,file=$WORKDIR/ovmf/OVMF_CODE.fd \
# 	-hda fat:rw:"$QEMU_ISO_PATH" \
# 	-serial stdio \
# 	-S -gdb tcp::1234

# -drive if=pflash,format=raw,file=$WORKDIR/ovmf/OVMF_VARS.fd \
# -drive if=pflash,format=raw,file=$HOME/.qemu/ovmf/edk2-i386-vars.fd

# #!/bin/bash
# set -e

# WORKDIR="$(pwd)"

# if [ -z "$1" ]; then
# 	echo "❌ Missing ISO directory"
# 	exit 1
# elif [ ! -d "$1" ]; then
# 	echo "❌ Directory not found: $1"
# 	exit 1
# else
# 	ISO_DIR="$1"
# fi

# case "$(uname -s)" in
# MINGW* | MSYS* | CYGWIN*)
# 	# Windows (WSL) - преобразуем путь
# 	WIN_ISO_DIR=$(cygpath -w "$ISO_DIR")
# 	QEMU_ISO_PATH="$WIN_ISO_DIR"
# 	;;
# Linux)
# 	QEMU_ISO_PATH="$ISO_DIR"
# 	;;
# Darwin)
# 	QEMU_ISO_PATH="$ISO_DIR"
# 	;;
# *)
# 	echo "❌ Unsupported OS: $(uname -s)"
# 	exit 1
# 	;;
# esac
#
# # Папка для OVMF
# OVMF_DIR="$HOME/.qemu/ovmf"
# mkdir -p "$OVMF_DIR"

# # Функция копирования OVMF из системных путей
# copy_ovmf_files() {
# 	local code_src=$1
# 	local vars_src=$2
# 	echo "📁 Copying OVMF files..."
# 	cp "$code_src" "$OVMF_DIR/OVMF_CODE.fd"
# 	cp "$vars_src" "$OVMF_DIR/OVMF_VARS.fd"
# 	echo "✅ OVMF files copied to $OVMF_DIR"
# }

# # macOS
# if [[ "$(uname -s)" == "Darwin" ]]; then
# 	# Путь в Homebrew QEMU
# 	BREW_QEMU_PREFIX=$(brew --prefix qemu)
# 	CODE_PATH="$BREW_QEMU_PREFIX/share/qemu/edk2-x86_64-code.fd"
# 	VARS_PATH="$BREW_QEMU_PREFIX/share/qemu/edk2-i386-vars.fd"
# 	copy_ovmf_files "$CODE_PATH" "$VARS_PATH"

# # Linux (Debian/Ubuntu или Arch)
# elif [[ "$(uname -s)" == "Linux" ]]; then
# 	if command -v apt-get &>/dev/null; then
# 		sudo apt-get update
# 		sudo apt-get install -y ovmf
# 		copy_ovmf_files "/usr/share/OVMF/OVMF_CODE.fd" "/usr/share/OVMF/OVMF_VARS.fd"
# 	elif command -v pacman &>/dev/null; then
# 		sudo pacman -Sy --noconfirm edk2-ovmf
# 		copy_ovmf_files "/usr/share/edk2-ovmf/x64/OVMF_CODE.fd" "/usr/share/edk2-ovmf/x64/OVMF_VARS.fd"
# 	else
# 		echo "❌ Unsupported Linux distro or no known package manager."
# 		exit 1
# 	fi

# # Windows (MSYS/Cygwin)
# elif [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
# 	echo "❌ Automatic OVMF install not supported on native Windows. Please install manually."
# 	exit 1
# fi

# # Запуск qemu
# echo "🚀 Running QEMU..."
# qemu-system-x86_64 \
# 	-M pc \
# 	-cpu Haswell \
# 	-smp 2 \
# 	-m 1024 \
# 	-drive if=pflash,format=raw,readonly=on,file="$OVMF_DIR/OVMF_CODE.fd" \
# 	-drive if=pflash,format=raw,file="$OVMF_DIR/OVMF_VARS.fd" \
# 	-hda fat:rw:"$QEMU_ISO_PATH" \
# 	-serial stdio
