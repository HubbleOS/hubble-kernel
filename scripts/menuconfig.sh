#!/usr/bin/env bash
#
# menuconfig.sh - Edit config.mk in menus, like the Linux kernel's
# `make menuconfig`, drawn with gum (https://github.com/charmbracelet/gum).
#
# Usage: make menuconfig   (or scripts/menuconfig.sh [CONFIG_FILE])
#
# Options keep their default (and stay commented out in config.mk) until
# they are set; clearing a value goes back to the default. Saving writes
# config.mk from config.mk.example, so it keeps the explanations, plus
# any other lines the old config.mk had.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${1:-$ROOT/config.mk}"
EXAMPLE="$ROOT/config.mk.example"

if ! command -v gum >/dev/null; then
    echo "menuconfig needs gum: https://github.com/charmbracelet/gum#installation"
    echo "(or edit config.mk by hand; config.mk.example explains every option)"
    exit 1
fi
if [ ! -t 0 ] || [ ! -t 1 ]; then
    echo "menuconfig needs a terminal"
    exit 1
fi

# -- Options -----------------------------------------------------------
# section|name|type|default|help
# type: tri (y/n/auto), mod (y/m/n), choice:a,b,c, num, text
# A section "Parent/child" is a submenu of Parent.
OPTIONS=(
    "Build|CROSS|text|x86_64-elf-|Cross toolchain prefix, or a full path to one"
    "Build|RELEASE|choice:0,1|0|1: release build, -O3 without debug info; 0: -Og with it"
    "Build|OPT_LEVEL|choice:g,0,1,2,3,s||-O level (empty: g, or 3 with RELEASE=1)"
    "Build|DEBUG_INFO|choice:y,n||Debug info for GDB (empty: y, or n with RELEASE=1)"
    "Build|JOBS|num||Parallel compile jobs (empty: number of CPUs)"
    "Kernel|CONFIG_LOGLEVEL|choice:0,1,2,3,4,5,6,7,8|8|Screen log level at boot: below it shows (8 all, 5 errors+warnings)"
    "Kernel|CONFIG_LOGLEVEL_SERIAL|choice:0,1,2,3,4,5,6,7,8|8|Serial log level"
    "Kernel|CONFIG_TIMER_HZ|num|100|Scheduler tick in Hz"
    "Modules/input|CONFIG_MOD_INPUT|mod|y|input.ko: PS/2 keyboard and mouse, evdev, PC speaker"
    "Modules/input|CONFIG_INPUT_KEYBOARD|choice:y,n|y|PS/2 keyboard"
    "Modules/input|CONFIG_INPUT_MOUSE|choice:y,n|y|PS/2 mouse"
    "Modules/input|MOD_INPUT_CFLAGS|text||Extra compiler flags for input.ko, e.g. -DDEBUG"
    "Modules/tty|CONFIG_MOD_TTY|mod|y|tty.ko: the console tty and ptys"
    "Modules/tty|MOD_TTY_CFLAGS|text||Extra compiler flags for tty.ko"
    "Modules/usb|CONFIG_MOD_USB|mod|y|usb.ko: USB keyboards and mice (after input)"
    "Modules/usb|CONFIG_USB_XHCI|choice:y,n|y|xHCI (USB 3) host controllers"
    "Modules/usb|MOD_USB_CFLAGS|text||Extra compiler flags for usb.ko"
    "Modules/procfs|CONFIG_MOD_PROCFS|mod|y|procfs.ko (Rust): /proc"
    "Modules/video|CONFIG_MOD_VIDEO|mod|y|video.ko: /dev/fb0 for the desktop"
    "Modules/video|MOD_VIDEO_CFLAGS|text||Extra compiler flags for video.ko"
    "Modules/net|CONFIG_MOD_NET|mod|m|net.ko: e1000 network card"
    "Modules/net|MOD_NET_CFLAGS|text||Extra compiler flags for net.ko"
    "Userland|MUSL_CC|text||Static musl compiler for demos and the GUI (empty: search PATH)"
    "Userland|MINGW_CC|text||mingw-w64 compiler for the Windows test programs"
    "Userland|CONFIG_BUSYBOX|tri|auto|Busybox in the image, booting busybox init"
    "Userland|BUSYBOX_DIR|text|\$(ROOT_DIR)/busy|Folder whose files go to the image root"
    "Userland|CONFIG_DEMOS|tri|auto|Linux-ABI demos (usr/demos) and /bin/peload"
    "Userland|CONFIG_GUI|tri|auto|The desktop and its GUI apps"
    "Userland|GUI_APPS|text|panel term|GUI apps to build (usr/apps/<name>)"
    "Userland|CONFIG_WIN|tri|auto|Windows test programs (usr/win), run by peload"
    "Test disk|CONFIG_DISK|tri|auto|out/disk.img: FAT32 at /fat, ext2 at /ext2"
    "Test disk|DISK_FAT_MB|num|64|FAT32 partition size in MB (at least 33)"
    "Test disk|DISK_EXT2_MB|num|64|ext2 partition size in MB"
    "QEMU|QEMU_ACCEL|choice:auto,kvm,hvf,tcg|auto|kvm (Linux), hvf (macOS), tcg (software)"
    "QEMU|QEMU_MEM|text|800M|Memory"
    "QEMU|QEMU_SMP|num|2|CPUs"
    "QEMU|QEMU_DISK|text|\$(ROOT_DIR)/out/disk.img|Disk for the IDE primary master"
    "QEMU|QEMU_DISPLAY|text||-display: gtk, sdl, cocoa, none (empty: QEMU's choice)"
    "QEMU|QEMU_SERIAL|text|stdio|-serial: stdio, none, file:PATH"
    "QEMU|QEMU_LOG|text|int|-d log items, e.g. int,cpu_reset (none: off)"
    "QEMU|QEMU_LOG_FILE|text|/tmp/qemu-kernel.log|File for the -d log"
    "QEMU|QEMU_NO_REBOOT|choice:y,n|y|Stop on a triple fault instead of rebooting"
    "QEMU|QEMU_NO_SHUTDOWN|choice:y,n|y|Keep QEMU open after a shutdown"
    "QEMU|QEMU_ARGS|text||Extra QEMU arguments"
    "Boot|CMDLINE|text||Kernel command line: loglevel=N quiet modules=a,b init=PATH"
    "Boot|INIT|text||First program (init=), e.g. /bin/sh (empty: /init, /sbin/init, ...)"
)
SECTIONS=("Build" "Kernel" "Modules" "Boot" "Userland" "Test disk" "QEMU")

declare -A TYPE DEFAULT HELP VALUE IS_SET
NAMES=()
for o in "${OPTIONS[@]}"; do
    IFS='|' read -r section name type default help <<<"$o"
    NAMES+=("$name")
    TYPE[$name]=$type
    DEFAULT[$name]=$default
    HELP[$name]=$help
    IS_SET[$name]=0
done

is_option() { [[ -n "${TYPE[$1]+x}" ]]; }

# -- Load config.mk ------------------------------------------------------
# "NAME := value" (or = / ?=) lines of known options; anything else that
# is not a comment is kept as it is.
EXTRA=()
if [ -f "$CONFIG" ]; then
    while IFS= read -r line || [ -n "$line" ]; do
        if [[ "$line" =~ ^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*(:=|\?=|=)[[:space:]]*(.*)$ ]] &&
            is_option "${BASH_REMATCH[1]}"; then
            VALUE[${BASH_REMATCH[1]}]="${BASH_REMATCH[3]%"${BASH_REMATCH[3]##*[![:space:]]}"}"
            IS_SET[${BASH_REMATCH[1]}]=1
        elif [[ -n "${line//[[:space:]]/}" && ! "$line" =~ ^[[:space:]]*# ]]; then
            EXTRA+=("$line")
        fi
    done <"$CONFIG"
fi

shown() { # the value in effect, for the menus
    if [ "${IS_SET[$1]}" = 1 ]; then
        echo "${VALUE[$1]}"
    elif [ -n "${DEFAULT[$1]}" ]; then
        echo "${DEFAULT[$1]}"
    else
        echo "(default)"
    fi
}

CHANGED=0

# -- Editing one option ------------------------------------------------
edit() {
    local name=$1 type=${TYPE[$1]} current new choices
    current=$(shown "$name")
    gum style --foreground 99 --bold -- "$name" >&2
    gum style --foreground 245 -- "${HELP[$name]}" >&2

    case $type in
    tri | mod | choice:*)
        case $type in
        tri) choices="y,n,auto" ;;
        mod) choices="y,m,n"
            gum style --foreground 245 -- "y: built and loaded at boot, m: built only, n: not built" >&2 ;;
        *) choices=${type#choice:} ;;
        esac
        IFS=',' read -ra list <<<"$choices"
        new=$(gum choose --header="$name" --selected="$current" -- "${list[@]}") || return 0
        ;;
    num | text)
        while :; do
            new=$(gum input --header="$name (empty: default${DEFAULT[$name]:+, ${DEFAULT[$name]}})" \
                --value="${VALUE[$name]:-}" --placeholder="${DEFAULT[$name]:-default}") || return 0
            if [ "$type" = num ] && [ -n "$new" ] && ! [[ "$new" =~ ^[0-9]+$ ]]; then
                gum style --foreground 196 -- "$name must be a number" >&2
                continue
            fi
            if [ "$name" = DISK_FAT_MB ] && [ -n "$new" ] && [ "$new" -lt 33 ]; then
                gum style --foreground 196 -- "FAT32 needs at least 33 MB" >&2
                continue
            fi
            break
        done
        ;;
    esac

    # Empty or the default: unset, so config.mk keeps the commented line.
    if [ -z "$new" ] || [ "$new" = "${DEFAULT[$name]}" ]; then
        [ "${IS_SET[$name]}" = 1 ] && CHANGED=1
        IS_SET[$name]=0
        unset 'VALUE[$name]'
    elif [ "${VALUE[$name]:-}" != "$new" ] || [ "${IS_SET[$name]}" = 0 ]; then
        VALUE[$name]=$new
        IS_SET[$name]=1
        CHANGED=1
    fi
}

# -- Menus ----------------------------------------------------------------
# Options set in a section and its submenus.
set_count() {
    local n=0 o s name
    for o in "${OPTIONS[@]}"; do
        IFS='|' read -r s name _ _ _ <<<"$o"
        [[ "$s" = "$1" || "$s" = "$1/"* ]] && [ "${IS_SET[$name]}" = 1 ] && n=$((n + 1))
    done
    echo $n
}

section_menu() {
    local section=$1 items choice last="" subs=() o s os name label first selected=""
    for o in "${OPTIONS[@]}"; do
        IFS='|' read -r s _ _ _ _ <<<"$o"
        if [[ "$s" = "$section/"* ]] && [[ " ${subs[*]-} " != *" ${s#"$section/"} "* ]]; then
            subs+=("${s#"$section/"}")
        fi
    done
    while :; do
        items=()
        for o in "${OPTIONS[@]}"; do
            IFS='|' read -r s name _ _ _ <<<"$o"
            [ "$s" = "$section" ] || continue
            local mark=" "
            [ "${IS_SET[$name]}" = 1 ] && mark="*"
            label=$(printf '%s %-22s %s' "$mark" "$name" "$(shown "$name")")
            items+=("$label|$name")
            [ "$name" = "$last" ] && selected=$label
        done
        # Submenus, with the value of their first option (a module's y/m/n).
        for s in ${subs[@]+"${subs[@]}"}; do
            first=""
            for o in "${OPTIONS[@]}"; do
                IFS='|' read -r os name _ _ _ <<<"$o"
                [ "$os" = "$section/$s" ] && { first=$name; break; }
            done
            local n mark=" "
            n=$(set_count "$section/$s")
            [ "$n" -gt 0 ] && mark="*"
            label=$(printf '%s %-22s %s' "$mark" "$s  >" "$(shown "$first")")
            items+=("$label|sub:$s")
            [ "sub:$s" = "$last" ] && selected=$label
        done
        items+=("  < Back|back")
        # The cursor stays on the option edited last.
        choice=$(gum choose --height 15 --label-delimiter "|" \
            --header="HubbleOS configuration › ${section//\// › }   (* = set in config.mk)" \
            ${last:+--selected="$selected"} -- "${items[@]}") || return 0
        [ "$choice" = back ] && return 0
        if [[ "$choice" = sub:* ]]; then
            section_menu "$section/${choice#sub:}"
        else
            edit "$choice"
        fi
        last=$choice
    done
}

save() {
    local tmp line name done_names=" "
    tmp=$(mktemp "$CONFIG.XXXXXX")
    while IFS= read -r line || [ -n "$line" ]; do
        # "# NAME := example": the option's place in the file.
        if [[ "$line" =~ ^#[[:space:]]([A-Za-z_][A-Za-z0-9_]*)[[:space:]]:= ]] &&
            is_option "${BASH_REMATCH[1]}"; then
            name=${BASH_REMATCH[1]}
            if [ "${IS_SET[$name]}" = 1 ] && [[ "$done_names" != *" $name "* ]]; then
                echo "$name := ${VALUE[$name]}"
                done_names+="$name "
                continue
            fi
        fi
        echo "$line"
    done <"$EXAMPLE" >"$tmp"

    # Set options the example has no line for, and the old file's other
    # lines.
    local more=()
    for name in "${NAMES[@]}"; do
        if [ "${IS_SET[$name]}" = 1 ] && [[ "$done_names" != *" $name "* ]]; then
            more+=("$name := ${VALUE[$name]}")
        fi
    done
    more+=("${EXTRA[@]+"${EXTRA[@]}"}")
    if [ ${#more[@]} -gt 0 ]; then
        printf '\n# Other settings\n' >>"$tmp"
        printf '%s\n' "${more[@]}" >>"$tmp"
    fi
    mv "$tmp" "$CONFIG"
}

last_section=""
while :; do
    items=()
    selected=""
    for s in "${SECTIONS[@]}"; do
        n=$(set_count "$s")
        label=$(printf '%-10s %s' "$s" "$([ $n -gt 0 ] && echo "($n set)")")
        items+=("$label|$s")
        [ "$s" = "$last_section" ] && selected=$label
    done
    items+=("Save and exit|save" "Exit without saving|quit")

    choice=$(gum choose --height 12 --label-delimiter "|" \
        --header="HubbleOS configuration ($(basename "$CONFIG"))$([ $CHANGED = 1 ] && echo '   [unsaved changes]')" \
        ${selected:+--selected="$selected"} -- "${items[@]}") || choice=quit

    case $choice in
    save)
        save
        gum style --foreground 76 "Saved $CONFIG"
        exit 0
        ;;
    quit)
        if [ $CHANGED = 1 ] && gum confirm "Save changes to $(basename "$CONFIG")?"; then
            save
            gum style --foreground 76 "Saved $CONFIG"
        fi
        exit 0
        ;;
    *)
        section_menu "$choice"
        last_section=$choice
        ;;
    esac
done
