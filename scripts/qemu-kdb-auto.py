#!/usr/bin/env python3
# QEMU wrapper that auto-triggers KDB after boot for semihosting
# Copyright (c) 2026 The F9 Microkernel Project

"""
Runs F9 under QEMU and automatically sends '?' to activate KDB
after detecting boot completion.

Usage:
    python3 scripts/qemu-kdb-auto.py [ELF_FILE] [--delay SECONDS]

This script solves the ARM semihosting limitation:
- Semihosting uses blocking I/O (no hardware interrupts)
- Cannot poll for '?' without freezing the system
- Solution: Automatically send '?' after boot detection
"""

import argparse
import os
import select
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description="Run F9 with auto-KDB trigger")
    parser.add_argument(
        "elf_file",
        nargs="?",
        default="build/b-l475e-iot01a/f9.elf",
        help="ELF file to run (default: build/b-l475e-iot01a/f9.elf)",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=2.0,
        help="Delay in seconds after boot before sending ? (default: 2.0)",
    )
    parser.add_argument(
        "--no-trigger",
        action="store_true",
        help="Do not auto-trigger KDB (manual mode)",
    )

    args = parser.parse_args()

    if not os.path.exists(args.elf_file):
        print(f"Error: ELF file not found: {args.elf_file}", file=sys.stderr)
        return 1

    print("=" * 60)
    print("F9 Microkernel - QEMU with KDB Auto-Trigger")
    print("=" * 60)
    print(f"ELF: {args.elf_file}")
    if not args.no_trigger:
        print(f"KDB trigger: {args.delay}s after boot")
    print()
    print("Press Ctrl+C to exit")
    print("=" * 60)
    print()

    qemu = os.environ.get("QEMU", "qemu-system-arm")
    cmd = [
        qemu,
        "-M",
        "b-l475e-iot01a",
        "-nographic",
        "-chardev",
        "stdio,id=console,mux=on,signal=off",
        "-serial",
        "chardev:console",
        "-mon",
        "chardev=console,mode=readline",
        "-semihosting-config",
        "enable=on,target=native,chardev=console",
        "-kernel",
        args.elf_file,
    ]

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=None if args.no_trigger else subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except FileNotFoundError:
        print("Error: qemu-system-arm not found", file=sys.stderr)
        return 127

    boot_detected = False
    start_time = time.time()
    kdb_triggered = args.no_trigger  # Skip trigger if --no-trigger
    line_buffer = ""

    try:
        while True:
            # Check if process exited
            if proc.poll() is not None:
                break

            # Read output with timeout
            ready, _, _ = select.select([proc.stdout], [], [], 0.1)
            if ready:
                try:
                    chunk = proc.stdout.read(1024)
                    if not chunk:
                        break

                    # Print output
                    sys.stdout.write(chunk)
                    sys.stdout.flush()

                    line_buffer += chunk

                    # Detect boot completion
                    if (
                        not boot_detected
                        and "Press '?' to print KDB menu" in line_buffer
                    ):
                        line_buffer = ""  # Free buffer after marker found
                        boot_detected = True
                        start_time = time.time()
                        if not kdb_triggered:
                            print(
                                f"\n[AUTO] Boot detected, waiting {args.delay}s before triggering KDB..."
                            )

                except (IOError, OSError):
                    pass

            # Auto-trigger KDB after delay
            if boot_detected and not kdb_triggered:
                elapsed = time.time() - start_time
                if elapsed >= args.delay:
                    print("[AUTO] Sending '?' to activate KDB...")
                    proc.stdin.write("?")
                    proc.stdin.flush()
                    kdb_triggered = True

    except KeyboardInterrupt:
        print("\n[QEMU] Interrupted by user")

    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()

    return proc.returncode or 0


if __name__ == "__main__":
    sys.exit(main())
