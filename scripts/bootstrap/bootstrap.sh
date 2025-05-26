#!/bin/bash
set -e

# Colors
GREEN="\033[0;32m"
RED="\033[0;31m"
RESET="\033[0m"

info() { echo -e "${GREEN}➤ $*${RESET}"; }
error() { echo -e "${RED}✖ $*${RESET}"; }

OS="$(uname -s)"

install_linux_deps() {
	info "Detected Linux"
	if command -v apt &>/dev/null; then
		sudo apt update
		sudo apt install -y \
			build-essential \
			qemu-system-x86 \
			mtools \
			xorriso \
			grub-pc-bin \
			grub-common \
			nasm \
			curl \
			wget \
			make
	else
		error "Unsupported Linux distro (only apt-based supported out-of-the-box)"
		exit 1
	fi
}

install_macos_deps() {
	info "Detected macOS"
	if ! command -v brew &>/dev/null; then
		info "Homebrew not found. Installing..."
		/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
		eval "$(/opt/homebrew/bin/brew shellenv)" # Apple Silicon fallback
	fi

	brew update
	brew install \
		qemu
	# mtools \
	# xorriso \
	# nasm \
	# make \
	# wget \
	# curl
}

install_wsl_hint() {
	info "Detected WSL"
	info "Installing Linux deps (APT-based)"
	install_linux_deps
	echo
	info "✅ NOTE: QEMU GUI will not work inside WSL. Use WSLg or run QEMU from Windows instead."
}

main() {
	case "$OS" in
	Linux)
		if grep -qi microsoft /proc/version; then
			install_wsl_hint
		else
			install_linux_deps
		fi
		;;
	Darwin)
		install_macos_deps
		;;
	*)
		error "Unsupported OS: $OS"
		exit 1
		;;
	esac

	info "✅ All dependencies installed successfully."
}

main "$@"
