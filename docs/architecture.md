<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

This is one small execution path, not a general OS framework. The authoritative
behavior and acceptance criteria remain in [requirements.toml](requirements.toml).

## Platform and Addresses

The tested command is provided by `cmake --build build --target run`:

```sh
qemu-system-aarch64 \
  -machine virt-10.1,gic-version=3,virtualization=off,secure=off,its=off \
  -cpu cortex-a710 -smp 1 -m 128M \
  -display none -serial stdio -monitor none -no-reboot \
  -kernel build/kernel.bin
```

The raw image enters at EL1 at `0x40080000`. Other entry levels are diagnosed and
halted, not normalized by extra firmware code. The only tested platform is QEMU
10.1.5's `virt-10.1`, without secure or virtualized execution.

| Region | Physical address / size |
| --- | --- |
| RAM | `0x40000000`, 128 MiB |
| Kernel entry | `0x40080000` |
| Kernel stack | 16 KiB after kernel BSS, below `0x41000000` |
| App 0 / 1 / 2 slot | `0x41000000` / `0x41020000` / `0x41040000`, 128 KiB each |
| App image | At slot base, at most 64 KiB |
| App stack | Last 16 KiB of each slot, growing down |
| PL011 | `0x09000000` |
| GICv3 distributor | `0x08000000` |
| CPU 0 redistributor / SGI frame | `0x080a0000` / `0x080b0000` |

App addresses come from [abi.h](../include/abi.h) in both linker and C code.
The kernel linker rejects overlap with app slots. Each app has its own ELF for
debugging; only its flat code/constants binary is embedded and copied. Writable
static storage, TLS, runtime initialization and oversized images fail at link time.
There is no ELF parser, relocation loader, or filesystem.

## Memory Management

Everything below is physical; there are no page tables yet. The layers follow
Linux and are built in that order, each on top of the previous one.

1. [memblock.c](../mm/memblock.c) describes RAM before any allocator exists. It
   holds two sorted, merged region arrays: available memory and reserved memory.
   `mem_init()` adds all of RAM, then reserves the firmware area and the kernel
   image up to `_end`, the three app slots, and the device tree blob. Walking
   memory minus reserved yields the free ranges.
2. [page_alloc.c](../mm/page_alloc.c) is a buddy allocator over a flat
   `struct page` array indexed by page frame number, itself allocated from
   memblock. `alloc_pages(order)` takes the smallest sufficient block and returns
   the unused halves to lower orders; `free_pages()` merges a block with the
   buddy that differs only in bit `order` of its page frame number, repeatedly,
   up to `MAX_ORDER - 1`.
3. [slab.c](../mm/slab.c) carves single pages into one power-of-two size class
   each, from 16 to 2048 bytes, chaining free objects through their own storage.
   `kmalloc` above 2048 bytes falls back to whole pages and records the order in
   the page, so `kfree` needs nothing but the pointer.

QEMU leaves the device tree pointer in `x0`; [boot.S](../kernel/boot.S) saves it
before anything else and `mem_init()` reserves the blob after checking its magic.
Nothing parses the tree yet, so RAM extent still comes from `abi.h`.

Because memory behaves as Device type while the MMU is off, every wide access
must be naturally aligned. [string.c](../lib/string.c) only widens to 64-bit
access when address and remaining length allow it, and free memory is never
bulk-zeroed at boot.

## Follow the Execution

1. [boot.S](../kernel/boot.S) masks exceptions, selects the EL1 stack, turns off
   MMU/caches, clears BSS, and installs the 2 KiB-aligned vector table.
2. [main.c](../kernel/main.c) prints actual CPU control state, then `mem_init()`
   ([init.c](../mm/init.c)) publishes the memory map and brings up the allocators.
   [task.c](../kernel/task.c) copies and compares each embedded app, clears its
   stack, and initializes its PC, SP and EL0t PSTATE.
3. [timer.c](../kernel/timer.c) initializes the GICv3 distributor, wakes the
   redistributor, enables Group 1 timer PPI 30, and enables the system-register
   CPU interface. `CNTFRQ_EL0 / 100` supplies a nominal 10 ms quantum.
4. [vectors.S](../kernel/vectors.S) enters EL0 using `ERET`. On `SVC` or IRQ it
   saves every integer register before C code can clobber it, plus `SP_EL0`,
   `ELR_EL1` and `SPSR_EL1`. C/assembly offsets are checked in
   [context.h](../kernel/context.h).
5. `trap()` handles a syscall or acknowledges the timer, rearms it and completes
   the interrupt. Timer IRQ selects the next runnable entry of a three-task array.
   `ERET` restores that task's registers, PC and flags and resumes EL0 with IRQ enabled.

One EL1 stack is sufficient because handlers do not block and IRQ stays masked
throughout kernel execution. Polling UART writes make each bounded log operation
serial, but also delay preemption while the kernel runs. This is not hard real-time
scheduling. Logging itself does not yield; timer IRQ, exit and fault can switch tasks.

A task that exits or faults is no longer runnable. If one remains it keeps running;
if none remain, the timer is disabled and the kernel prints `ALL APPS DONE` and
idles. A kernel exception reports ESR/ELR and halts. QEMU must be stopped separately.

## System Calls

`SVC #0` uses `x8` for the call number, `x0`/`x1` for arguments and `x0` for the
signed result. Other registers and condition flags survive a returning syscall.

| Number | Call | Result |
| --- | --- | --- |
| 0 | `log(pointer, length)` | Payload byte count, or negative error |
| 1 | `exit(status)` | Records status; never returns |

Logs accept 0 to 256 bytes from the current app's actual image or stack. Zero
length never dereferences the pointer. The `[app N] ` prefix is not counted in
the result. Overflow-safe bounds checks reject crossing ranges and other task,
kernel or MMIO buffers. Errors are `-14` (`EFAULT`), `-22` (`EINVAL`, excessive
length), and `-38` (`ENOSYS`, unknown call).

[app.h](../apps/app.h) contains the small register-based wrapper and stack-based
integer formatting. [start.S](../apps/start.S) calls `app_main()` and exits with
its return value. There is no user libc or `printf` for applications. Kernel
diagnostics go through `printk` ([printk.c](../lib/printk.c)) over the polling
UART, while log syscall payloads stay byte-exact and bypass the formatter.

## Limits and Evidence

EL0 prohibits privileged instructions, but MMU-off execution has no memory
isolation, guard pages, or stack protection. Only trusted apps are supported.
Applications get no memory system calls; the allocators serve the kernel only.
All code is compiled with `-mgeneral-regs-only`; floating point, SIMD and SVE
contexts are deliberately unsupported. ELF segment flags do not enforce memory
permissions while the MMU is off. Caches stay off to keep copied-code startup simple.

[test_kernel.py](../tests/test_kernel.py) builds real fixtures and captures bounded
QEMU serial output. Three no-SVC loops prove timer-driven progress and register/
flag preservation over repeated resumes; alternating demo log lines alone do not.
Other checks exercise syscall returns, faults, linker rejection, incremental
repackaging, and all runnable-task combinations. Tests use separate build directories
so instrumentation does not modify the normal demo image.