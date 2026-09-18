# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import os
import re
import selectors
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

from tests.support import verifies


ROOT = Path(__file__).resolve().parents[1]
QEMU = [
    "qemu-system-aarch64", "-machine",
    "virt-10.1,gic-version=3,virtualization=off,secure=off,its=off",
    "-cpu", "cortex-a710", "-smp", "1", "-m", "128M",
    "-display", "none", "-serial", "stdio", "-monitor", "none", "-no-reboot",
]


def build(directory, *options):
    for command in (
        ["cmake", "-S", str(ROOT), "-B", str(directory), "-G", "Ninja",
         *(f"-D{option}" for option in options)],
        ["cmake", "--build", str(directory), "--parallel", "2"],
    ):
        result = subprocess.run(
            command, cwd=ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, timeout=60,
        )
        if result.returncode:
            raise AssertionError(result.stdout)


def emulate(image, marker, extra=(), occurrences=1):
    output = bytearray()
    with subprocess.Popen(
        [*QEMU, *extra, "-kernel", str(image)], cwd=ROOT,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    ) as process:
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                deadline = time.monotonic() + 10
                while output.count(marker.encode()) < occurrences:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0 or not selector.select(remaining):
                        raise AssertionError(f"QEMU timed out:\n{output.decode(errors='replace')}")
                    chunk = os.read(process.stdout.fileno(), 4096)
                    if not chunk:
                        raise AssertionError(f"QEMU ended:\n{output.decode(errors='replace')}")
                    output.extend(chunk)
        finally:
            process.terminate()
            try:
                process.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
    return output.decode(errors="replace")


class KernelTests(unittest.TestCase):
    @verifies("REQ-BOOT-001", "REQ-PRINT-001")
    def test_boot_el1_mmu_off(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory, "EXTRA_CFLAGS=-DTEST_DIRTY_BSS")
            output = emulate(Path(directory) / "kernel.bin", "BOOT OK\n")
        match = re.search(r"BOOT EL=([0-9a-f]+) SCTLR=([0-9a-f]+) VBAR=([0-9a-f]+) BSS=([0-9a-f]+) SP=([0-9a-f]+)", output)
        self.assertIsNotNone(match, output)
        level, control, vectors, bss, stack = (int(value, 16) for value in match.groups())
        self.assertEqual(level, 1)
        self.assertEqual(control & 0x1005, 0)
        self.assertEqual(vectors % 2048, 0)
        self.assertGreaterEqual(vectors, 0x40080000)
        self.assertEqual(bss, 0)
        self.assertEqual(stack % 16, 0)
        self.assertGreater(stack, vectors)
        self.assertLess(stack, 0x41000000)

    @verifies("REQ-APP-001", "REQ-SYS-001", "REQ-BUILD-001", "REQ-PRINT-001")
    def test_independent_demos(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory)
            for index in range(3):
                for suffix in ("elf", "bin"):
                    self.assertTrue((Path(directory) / f"apps/{index}.{suffix}").is_file())
            output = emulate(Path(directory) / "kernel.bin", "ALL APPS DONE\n")
        self.assertNotIn("PANIC", output)
        self.assertNotIn("FAULT", output)
        for index in range(3):
            self.assertIn(f"LOADED app={index} image={0x41000000 + index * 0x20000:016x} stack={0x41020000 + index * 0x20000:016x} copy=OK", output)
            self.assertEqual(output.count(f"[app {index}] EXIT status=0"), 1)
        self.assertIn("[app 0] sequence result=4500001500000\n", output)
        self.assertIn("[app 1] primes result=9592\n", output)
        self.assertIn("[app 2] checksum result=510000000\n", output)

    @verifies("REQ-SCHED-001", "REQ-EXC-001", "REQ-PRINT-001")
    def test_preempts_without_syscalls(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory, "EXTRA_CFLAGS=-DTEST_PREEMPT",
                  *(f"APP{index}=tests/fixtures/spin.S" for index in range(3)))
            output = emulate(Path(directory) / "kernel.bin", "context=OK\n", occurrences=9)
        self.assertNotIn("PANIC", output)
        ticks = re.findall(r"TICK app=(\d) progress=([0-9a-f]+) context=OK", output)
        self.assertEqual([int(task) for task, _ in ticks[:9]], [0, 1, 2] * 3)
        for task in range(3):
            counts = [int(count, 16) for identifier, count in ticks if int(identifier) == task]
            self.assertGreater(counts[0], 0)
            self.assertTrue(all(after > before for before, after in zip(counts, counts[1:])))
            self.assertIn(f"[app {task}] TRAP origin=0000000000000000 handler=0000000000000001", output)
        frequency, quantum = re.search(r"TIMER frequency=([0-9a-f]+) quantum=([0-9a-f]+)", output).groups()
        self.assertEqual(int(quantum, 16), int(frequency, 16) // 100)

    @verifies("REQ-SYS-001", "REQ-EXC-001", "REQ-SCHED-001", "REQ-PRINT-001")
    def test_syscalls_and_task_fault(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory, "APP0=tests/fixtures/syscalls.c", "APP1=tests/fixtures/fault.c")
            output = emulate(Path(directory) / "kernel.bin", "ALL APPS DONE\n")
        self.assertNotIn("PANIC", output)
        self.assertNotIn("ERROR", output)
        self.assertIn("[app 0] " + "Z" * 256 + "[app 0] \n", output)
        self.assertIn("[app 0] syscalls OK\n", output)
        self.assertIn("[app 0] EXIT status=0", output)
        self.assertRegex(output, r"\[app 1\] FAULT ESR=[0-9a-f]{16} ELR=000000004102[0-9a-f]{4}")
        self.assertNotIn("[app 1] EXIT", output)
        self.assertIn("[app 2] checksum result=510000000", output)
        self.assertIn("[app 2] EXIT status=0", output)

    @verifies("REQ-EXC-001", "REQ-SYS-001")
    def test_syscall_preserves_registers(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory, *(f"APP{index}=tests/fixtures/syscall_registers.S" for index in range(3)))
            output = emulate(Path(directory) / "kernel.bin", "ALL APPS DONE\n")
        self.assertNotIn("PANIC", output)
        self.assertNotIn("FAULT", output)
        for index in range(3):
            self.assertIn(f"[app {index}] EXIT status=0", output)

    @verifies("REQ-EXC-001", "REQ-PRINT-001")
    def test_kernel_fault_halts(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory, "EXTRA_CFLAGS=-DTEST_KERNEL_FAULT")
            output = emulate(Path(directory) / "kernel.bin", "PANIC EL1 ESR=0000000002000000 ELR=", occurrences=1)
        self.assertNotIn("LOADED", output)
        self.assertNotIn("ALL APPS DONE", output)

    @verifies("REQ-BOOT-001")
    def test_rejects_el2_entry(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory)
            output = emulate(Path(directory) / "kernel.bin", "PANIC unsupported entry EL\n",
                             extra=("-machine", "virtualization=on"))
        self.assertNotIn("BOOT OK", output)

    @verifies("REQ-BUILD-001")
    def test_rejects_unsupported_app_sections(self):
        for case in range(1, 7):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as directory:
                with self.assertRaisesRegex(AssertionError, "Application (static storage|image exceeds slot)"):
                    build(directory, "APP0=tests/fixtures/forbidden.c", f"EXTRA_CFLAGS=-DCASE={case}")

    @verifies("REQ-BUILD-001")
    def test_repackages_changed_app(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "app.c"
            source.write_text("int app_main(void) { return 0; }\n")
            build(directory, f"APP0={source}")
            image = Path(directory) / "kernel.bin"
            before = image.read_bytes()
            app_before = (Path(directory) / "apps/0.bin").read_bytes()
            artifacts = [Path(directory) / name for name in (
                "kernel.elf", "kernel.bin", "apps/0.elf", "apps/0.bin",
                "apps/1.elf", "apps/1.bin", "apps/2.elf", "apps/2.bin",
            )]
            timestamps = [artifact.stat().st_mtime_ns for artifact in artifacts]
            build(directory, f"APP0={source}")
            self.assertEqual(
                [artifact.stat().st_mtime_ns for artifact in artifacts], timestamps,
            )
            source.write_text("int app_main(void) { return 7; }\n")
            build(directory, f"APP0={source}")
            self.assertNotEqual((Path(directory) / "apps/0.bin").read_bytes(), app_before)
            self.assertNotEqual(image.read_bytes(), before)
            self.assertEqual(
                [artifact.stat().st_mtime_ns for artifact in artifacts[4:]], timestamps[4:],
            )
            output = emulate(image, "ALL APPS DONE\n")
        self.assertIn("[app 0] EXIT status=7\n", output)

    @verifies("REQ-BUILD-001")
    def test_reconfigures_app_and_flags(self):
        with tempfile.TemporaryDirectory(prefix="miaow build ") as directory:
            source = Path(directory) / "replacement.c"
            source.write_text("int app_main(void) { return APP_STATUS; }\n")
            build(directory)
            for status in (7, 9):
                build(directory, f"APP0={source}", f"EXTRA_CFLAGS=-DAPP_STATUS={status}")
                output = emulate(Path(directory) / "kernel.bin", "ALL APPS DONE\n")
                self.assertIn(f"[app 0] EXIT status={status}\n", output)
                self.assertNotIn("sequence result=", output)

    @verifies("REQ-PRINT-001")
    def test_formatter_on_host(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "format"
            result = subprocess.run(
                ["cc", "-std=c23", "-Wall", "-Wextra", "-Werror", "-ffreestanding",
                 "-fno-builtin", "-Iinclude", "lib/vsprintf.c", "tests/fixtures/format.c",
                 "-o", str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("FORMAT OK\n", result.stdout)

    @verifies("REQ-SCHED-001")
    def test_scheduler_all_states(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "scheduler"
            result = subprocess.run(
                ["cc", "-std=c23", "-Wall", "-Wextra", "-Werror", "-Iinclude", "-Ikernel",
                 "tests/fixtures/scheduler.c", "-o", str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)

    @verifies("REQ-SCHED-001")
    def test_single_survivor_and_spurious_interrupt(self):
        with tempfile.TemporaryDirectory() as directory:
            build(directory, "EXTRA_CFLAGS=-DTEST_SCHED_TRACE -DTEST_SPURIOUS",
                  "APP0=tests/fixtures/syscall_registers.S", "APP1=tests/fixtures/fault.c",
                  "APP2=tests/fixtures/spin.S")
            output = emulate(Path(directory) / "kernel.bin", "SWITCH 2 -> 2\n", occurrences=3)
        self.assertNotIn("PANIC", output)
        self.assertIn("SPURIOUS OK\n", output)
        self.assertIn("[app 0] EXIT status=0", output)
        self.assertIn("[app 1] FAULT", output)
        switches = re.findall(r"SWITCH (\d) -> (\d)", output)
        survivor = switches.index(("2", "2"))
        self.assertTrue(all(pair == ("2", "2") for pair in switches[survivor:]))
        self.assertNotIn("ALL APPS DONE", output)