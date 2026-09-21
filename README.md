# miaow-os

An operating-system learning project targeting QEMU AArch64. Development is incremental and requirement-driven with AI agents. The target is Cortex-A710 (Armv9-A) with GICv3 on QEMU, using one CPU, 128 MiB RAM, and a headless serial console.

**Current state:** a bootable EL1 kernel with three independently built EL0 demo applications, GICv3 timer preemption, and logging/exit system calls. Physical memory is managed by a memblock-style early allocator, a buddy page allocator, and kmalloc-style slabs; the MMU stays off. Linked QEMU tests exercise boot, applications, context switching, and error paths.

See [Architecture](docs/architecture.md) for the execution path, address map, ABI, and a short source-reading order.

## Quick Start

Requires CMake 3.20+, Ninja, C23-capable AArch64 GCC/binutils, QEMU with Cortex-A710 and `virt-10.1` support, a C23-capable host `cc` for the scheduler test, and Python
3.11+ with `venv`. On Fedora, missing tools can be installed manually with 

```sh
sudo dnf install cmake ninja-build gcc gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-system-aarch64
```

Set up the Python virtual environment from the repository root:

```sh
python3 -m venv .venv
```

From the repository root:

```sh
cmake -S . -B build -G Ninja
cmake --build build
cmake --build build --target run
```

The apps run and exits with status zero. Log order varies with timer delivery. After `ALL APPS DONE`, the timer is disabled and the kernel idles; QEMU remains running. Press Ctrl-C to stop.

## Configure

The default output is `build/kernel.bin`, with symbols in `build/kernel.elf` and independent app ELF/bin files under `build/apps/`.
The default [toolchain file](cmake/aarch64.cmake) uses the `aarch64-linux-gnu-` prefix. Select another compatible prefix with `-DCROSS_COMPILE=...` on the initial configure, or supply your own `-DCMAKE_TOOLCHAIN_FILE=...`.

Application sources and extra compiler flags are CMake cache options. They can be changed by reconfiguring the same build directory:

```sh
cmake -S . -B build/preempt -G Ninja \
	-DAPP0=tests/fixtures/spin.S \
	-DAPP1=tests/fixtures/spin.S \
	-DAPP2=tests/fixtures/spin.S \
	-DEXTRA_CFLAGS=-DTEST_PREEMPT
cmake --build build/preempt --target run
```

### Testing

From the repository root:

```sh
source .venv/bin/activate
python -m unittest tests.test_kernel -v
python tools/check.py
```

The check validates requirements and test links, executes tests, and writes the current traceability report to `build/test-results.json`. 

## Clean

To clean the build outputs, use:

```sh
cmake --build build --target clean
```

## Working With Agents

[AGENTS.md](AGENTS.md) is the sole project instruction source. 

Describe the next capability and constraints. The agent updates requirements, implementation, tests, and relevant docs.

- [Development workflow](docs/development.md): schema, tests, completion, licensing.
- [Requirement register](docs/requirements.toml): authoritative acceptance criteria.
- [Tooling tests](tests/test_workflow.py): executable examples of traceability.
- [Kernel tests](tests/test_kernel.py): bounded QEMU and host behavioral checks.

## License

Original project content: GPL-3.0-or-later. Third-party content retains its own or compatible license. Check [the third-party codes](docs/third-party.md) for details.
