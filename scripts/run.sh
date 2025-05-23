#!/bin/bash
set -e

case "$1" in
flash)
	sh "$(dirname "$0")/img/make-flash.sh"
	;;
qemu)
	sh "$(dirname "$0")/qemu/qemu.sh"
	;;
help)
	sh "$(dirname "$0")/cli/make-help.sh"
	;;
img)
	sh "$(dirname "$0")/img/make-image.sh"
	;;
*)
	echo "Unknown command: $1"
	exit 1
	;;
esac
