<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# miaow-os

A solo operating-system learning project targeting QEMU AArch64. Development is
incremental and requirement-driven, with AI agents assisting implementation and
explanation. The kernel baseline is freestanding C23 with minimal AArch64 assembly.

**Current state:** a bootable EL1 kernel with three independently built EL0 demo
applications, GICv3 timer preemption, and logging/exit system calls. Linked QEMU
tests exercise boot, applications, context switching, and error paths.

## First OS Milestone

The target is **Cortex-A710 (Armv9-A) with GICv3** on QEMU `virt`, using one
CPU, 128 MiB RAM, and a headless serial console. This replaces the earlier
Cortex-A53/GICv2 proposal. The supported machine is pinned to `virt-10.1`.

The implementation is for learning: follow the execution path from boot to an
application, exception entry, task selection, and exception return. Use direct
functions, a fixed task array, and a single kernel exception stack. Do not add
generic driver interfaces, pluggable schedulers, or a general-purpose loader.

- The kernel runs at EL1; applications run at EL0. Initially MMU and caches are off.
- Three apps are independently linked at fixed physical addresses. Their binary
	payloads are embedded in the boot image and copied by the kernel into separate
	slots, each with its own aligned stack. Replacing an app requires repackaging.
- Apps may use string constants and stack-local variables. Writable global/static
	storage, TLS, a heap, and a hosted C runtime are not supported.
- A physical timer preempts apps approximately every 10 ms. Round-robin selection
	skips finished tasks; no priorities, blocking queues, or cooperative yield API.
- Apps use `log(pointer, length)` and `exit(status)` through `SVC #0`. The kernel
	writes logs to PL011; demos do not access UART directly.
- Save all integer registers, the user stack pointer, return PC, and saved PSTATE.
	Compile with general-purpose registers only; no FP, SIMD, or SVE support yet.

GICv3 needs distributor setup, the single CPU's redistributor for timer PPI 30,
and the `ICC_*_EL1` system-register interface for interrupt delivery/completion.
Only this path is needed: no ITS, LPI, SMP, nested interrupts, or virtualization.
GICv4's virtual-interrupt features do not help this milestone. A newer CPU does
not require enabling optional features such as MTE or SVE.

**No memory isolation:** EL0 restricts privileged instructions, but without an MMU
an app can corrupt kernel or other app memory. Separate slots and syscall pointer
checks are correctness measures, not a security boundary. Run trusted demos only.

Build incrementally: EL1 serial boot, one loaded EL0 app with syscalls, timer-driven
switching, then the sequence/prime/checksum demos. Test no-syscall busy loops to
prove preemption, register patterns to prove context restoration, and invalid
syscall inputs and task exits to exercise boundary cases. Keep tests in the
existing Python runner; do not add a separate test framework.

See [Architecture](docs/architecture.md) for the execution path, address map, ABI,
and a short source-reading order. The fixed device addresses are tested on
`virt-10.1`; they are not a portability guarantee for unversioned QEMU `virt`.

## Quick Start

Requires GNU Make, C23-capable AArch64 GCC/binutils, QEMU with Cortex-A710 and
`virt-10.1` support, a C23-capable host `cc` for the scheduler test, and Python
3.11+ with `venv`. No third-party Python packages are needed. On Fedora, missing
tools can be installed manually with `sudo dnf install make gcc gcc-aarch64-linux-gnu
binutils-aarch64-linux-gnu qemu-system-aarch64 python3`.

Run from the repository root:

```sh
python3 -m venv .venv
make
make run
```

The demos report sequence result `4500001500000`, prime count `9592`, and checksum
`510000000`, then each exits with status zero. Log order varies with timer delivery.
After `ALL APPS DONE`, the timer is disabled and the kernel idles; QEMU remains
running. Press Ctrl-C in the terminal to stop it.

```sh
.venv/bin/python -m unittest tests.test_kernel -v
.venv/bin/python tools/check.py
```

The check validates requirements and test links, executes tests, and writes the
current traceability report to `build/test-results.json`. Exit zero means the gate
passed; planned requirements can remain pending. `.venv` and `build` are ignored.
If Python or `venv` is unavailable, install it through your OS package manager first.
Agents must leave system installation to you.

The default output is `build/os/kernel.bin`, with kernel symbols in
`build/os/kernel.elf` and independent app ELF/bin files under `build/os/apps/`.
`CROSS_COMPILE` defaults to `aarch64-linux-gnu-`. An alternate compatible toolchain
can be selected explicitly. Use a fresh `BUILD` directory when changing compiler
flags, toolchains, or `APP0`/`APP1`/`APP2` overrides; ordinary source edits rebuild
their objects and repack the image automatically.

## Working With Agents

[AGENTS.md](AGENTS.md) is the sole project instruction source. Use an agent with
root `AGENTS.md` support, including current VS Code Copilot, or explicitly ask the
tool to read it before working. Automatic loading depends on the tool and settings.
In VS Code, keep `chat.useAgentsMdFile` enabled and check a new chat's instruction
references or customization diagnostics for this file. Merely creating it does
not prove a particular chat loaded it; no editor settings are changed by this repo.

Describe the next capability and constraints. The agent updates requirements,
implementation, tests, and relevant docs together, then reports actual evidence.
Git commits and pushes remain explicit user actions.

- [Development workflow](docs/development.md): schema, tests, completion, licensing.
- [Requirement register](docs/requirements.toml): authoritative acceptance criteria.
- [Tooling tests](tests/test_workflow.py): executable examples of traceability.
- [Kernel tests](tests/test_kernel.py): bounded QEMU and host behavioral checks.

Kernel work needs GNU Make, C23-capable AArch64 GCC/binutils, and
`qemu-system-aarch64`; an AArch64-capable GDB is optional. An available
`aarch64-linux-gnu-` toolchain can build freestanding code when explicitly linked
without Linux startup files or libraries; a new `aarch64-none-elf-` installation
is not inherently necessary.

Verified on Fedora 43 WSL on 2026-09-17: QEMU 10.1.5,
`aarch64-linux-gnu-gcc` 15.2.1, GNU binutils 2.45, host GCC 15.3.1, and Python
3.14.7. The actual kernel/apps build uses `-std=c23 -mcpu=cortex-a710` and
checks `__STDC_VERSION__ >= 202311L`, not a fallback to an older C standard.

## License

Original project content: GPL-3.0-or-later, by miaow <guoyr_2013@hotmail.com>.
See [the full GPLv3 text](LICENSES/GPL-3.0-or-later.txt) and per-file SPDX notices.
Third-party content retains its own license; compatibility must be checked before
incorporation or redistribution. No third-party implementation is currently vendored.