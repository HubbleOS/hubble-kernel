import re
import os
import sys
import argparse
import subprocess
from pathlib import Path
import signal
import termios
import atexit

orig_settings = termios.tcgetattr(sys.stdin)


def restore_terminal():
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, orig_settings)


atexit.register(restore_terminal)


def signal_handler(sig, frame):
    restore_terminal()
    sys.exit(0)


signal.signal(signal.SIGINT, signal_handler)


class QemuOptions:
    def __init__(self, arch="x86_64", mem=256, smp=1, debug_port=1000,
                 iso=None):
        self.arch = arch
        self.mem = mem
        self.smp = smp
        self.debug_port = debug_port
        self.iso = iso


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Run QEMU with Hubble OS ISO")
    parser.add_argument("--iso", type=str,
                        help="Path to bootable ISO file")
    parser.add_argument("--arch", type=str,
                        help="Architecture, e.g., x86_64")
    parser.add_argument("--mem", type=int, help="Memory in MB")
    parser.add_argument("--smp", type=int, help="Number of CPU cores")
    parser.add_argument("--debug", type=int, help="Debug port")
    args = parser.parse_args()
    opts = QemuOptions()

    # Default ISO path
    if args.iso:
        opts.iso = args.iso
    else:
        opts.iso = str(Path("out/hubble.iso").resolve())

    if args.arch:
        opts.arch = args.arch
    if args.mem:
        opts.mem = args.mem
    if args.smp:
        opts.smp = args.smp
    if args.debug:
        opts.debug_port = args.debug
    return opts


def get_exe_dir():
    return Path(__file__).resolve().parent


def build_qemu_command(opts: QemuOptions):
    exe_dir = get_exe_dir()
    ovmf_path = exe_dir / "ovmf" / "OVMF_CODE.fd"
    if not ovmf_path.exists():
        raise FileNotFoundError(f"OVMF_CODE.fd not found in {ovmf_path}")

    iso_path = Path(opts.iso).resolve()
    if not iso_path.exists():
        print(f"Error: ISO file not found: {iso_path}")
        sys.exit(1)

    cmd = [
        f"qemu-system-{opts.arch}",
        "-M", "pc",
        "-cpu", "Haswell",
        "-m", str(opts.mem),
        "-smp", str(opts.smp),

        # Boot from ISO image (El Torito / UEFI CD-ROM)
        "-cdrom", str(iso_path),

        # OVMF firmware (Limine boots through UEFI)
        "-drive", f"if=pflash,format=raw,readonly=on,file={ovmf_path}",
    ]

    # Optional persistent disk (created by 'make disk')
    disk_img = Path("out/disks/disk.img").resolve()
    if disk_img.exists():
        cmd.extend([
            "-drive", f"file={disk_img},format=raw,index=1,media=disk,cache=none",
        ])

    cmd.extend([
        # Serial
        "-serial", "stdio",

        # Network
        "-netdev", "user,id=net0,hostfwd=udp::4444-:7777",
        "-device", "e1000,netdev=net0",

        # Debug
        "-d", "int",
        "-D", "/tmp/qemu.log",
        "-no-reboot", "-no-shutdown"
    ])

    return cmd


def run_qemu(opts: QemuOptions):
    iso_path = Path(opts.iso).resolve()
    if not iso_path.exists():
        print(f"Error: ISO not found: {iso_path}")
        print("Run 'make iso' first to generate the ISO.")
        sys.exit(1)

    cmd = build_qemu_command(opts)
    print(" ".join(cmd))
    print()

    process = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    for line in process.stdout:
        sys.stdout.buffer.write(line)
        sys.stdout.buffer.flush()

    return process.wait()


if __name__ == "__main__":
    options = parse_arguments()
    sys.exit(run_qemu(options))
