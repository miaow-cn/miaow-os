# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Print AArch64 registers and memory from a freshly reset QEMU machine.

Usage:
    .venv/bin/python tools/get.py tcr2_el1 ttbr0_el1 0x08000000

Starts the project QEMU machine with no kernel and halted at reset, attaches
GDB, and prints one line per requested item. An item is a memory address when
it parses as a number (8 bytes are read, little endian); otherwise it is a
register name, tried as written, uppercased, and without an "_EL1" suffix, so
"sp", "TTBR0_EL1", and "tcr2_el1" all work. Unknown or unreadable items print
"unavailable". Set GDB in the environment to select another debugger.
"""

import os
import socket
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
QEMU = [
    "qemu-system-aarch64", "-machine",
    "virt-10.1,gic-version=3,virtualization=off,secure=off,its=off",
    "-cpu", "cortex-a710", "-smp", "1", "-m", "128M",
    "-display", "none", "-serial", "none", "-monitor", "none", "-no-reboot", "-S",
]
SCRIPT = """
import os
import gdb


def read(item):
    try:
        address = int(item, 0)
    except ValueError:
        pass
    else:
        try:
            data = gdb.selected_inferior().read_memory(address, 8)
        except gdb.MemoryError:
            return None
        return int.from_bytes(data, "little")
    for name in (item, item.upper(), item.upper().removesuffix("_EL1")):
        try:
            return int(gdb.parse_and_eval("$" + name)) & (2 ** 64 - 1)
        except gdb.error:
            continue
    return None


with open(os.environ["GET_OUTPUT"], "w") as output:
    for item in os.environ["GET_ITEMS"].split():
        value = read(item)
        print(f"{item} = {'unavailable' if value is None else format(value, '#018x')}", file=output)
"""


def main(items):
    if not items:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    with tempfile.TemporaryDirectory() as directory:
        script = Path(directory) / "get_checks.py"
        script.write_text(SCRIPT)
        results = Path(directory) / "results.txt"
        command = [
            os.environ.get("GDB", "gdb"), "-nx", "-q", "-batch",
            "-ex", "set pagination off", "-ex", "set confirm off",
            "-ex", "set tcp auto-retry on", "-ex", "set tcp connect-timeout 5",
            "-ex", f"target remote 127.0.0.1:{port}",
            "-ex", f"source {script}",
        ]
        with subprocess.Popen([*QEMU, "-gdb", f"tcp:127.0.0.1:{port}"], cwd=ROOT,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as qemu:
            try:
                result = subprocess.run(
                    command, cwd=ROOT, capture_output=True, text=True, timeout=20,
                    env={**os.environ, "GET_ITEMS": " ".join(items), "GET_OUTPUT": str(results)},
                )
            finally:
                qemu.terminate()
        if not results.exists():
            print(result.stdout + result.stderr, file=sys.stderr)
            return result.returncode or 1
        print(results.read_text(), end="")
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
