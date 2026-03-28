import os
import sys
import argparse
import subprocess
from pathlib import Path


class QemuOptions:
    def __init__(self, arch="x86_64", mem=256, smp=2, debug_port=1000, iso_path="out/build/x86/iso/"):
        self.arch = arch
        self.mem = mem
        self.smp = smp
        self.debug_port = debug_port
        self.iso_path = iso_path


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Run QEMU with specified options")
    parser.add_argument("--iso", type=str, help="Path to ISO folder")
    parser.add_argument("--arch", type=str, help="Architecture, e.g., x86_64")
    parser.add_argument("--mem", type=int, help="Memory in MB")
    parser.add_argument("--smp", type=int, help="Number of CPU cores")
    parser.add_argument("--debug", type=int, help="Debug port")
    args = parser.parse_args()
    opts = QemuOptions()
    if args.iso:
        opts.iso_path = args.iso
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
    # Current working directory
    return Path(__file__).resolve().parent


def build_qemu_command(opts: QemuOptions):
    exe_dir = get_exe_dir()
    ovmf_path = exe_dir / "ovmf" / "OVMF_CODE.fd"
    if not ovmf_path.exists():
        raise FileNotFoundError(f"OVMF_CODE.fd not found in {ovmf_path}")

    cmd = [
        f"qemu-system-{opts.arch}",
        "-M", "pc",
        # "-M ", "q35",
        "-cpu", "Haswell",
        "-m", str(opts.mem),
        "-smp", str(opts.smp),

        # Main disk
        "-drive", "file=out/disks/disk.img,format=raw,index=0,media=disk,cache=none",

        # ISO
        # // pos += snprintf(cmd + pos, len - pos,
        #                    // "-drive file=fat:rw:%s,if=none,id=nvm-1 "
        #                    // "-device nvme,drive=nvm-1,serial=nvme-test ",
        #                    // opts -> iso_path)
        "-drive", f"file=fat:rw:{opts.iso_path},format=raw,index=1,media=disk",

        # OVMF
        "-drive", f"if=pflash,format=raw,readonly=on,file={ovmf_path}",

        # Serial
        "-serial", "stdio",

        # Internet
        "-netdev user,id=net0,hostfwd=udp::4444-:7777",
        "-device e1000,netdev=net0",

        # Debugging
        # "-S", "-s", "-d", "cpu_reset", "-no-reboot", "-no-shutdown"
    ]
    return " ".join(cmd) + " | sed 's/\\x1b\\[[0-9;]*m//g'"


def run_qemu(opts: QemuOptions):
    if not Path(opts.iso_path).exists():
        print(f"ISO directory not found: {opts.iso_path}")
        sys.exit(1)
    cmd = build_qemu_command(opts)
    print(f"Running: {cmd}")
    return subprocess.call(cmd, shell=True)


if __name__ == "__main__":
    options = parse_arguments()
    sys.exit(run_qemu(options))
