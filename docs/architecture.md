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

The MMU is enabled before entering `kernel_main()`, but all mapped virtual
addresses still equal their physical addresses. The allocator layers follow
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

During early startup, before enabling the MMU, data accesses behave as Device
type and wide accesses must be naturally aligned. Startup uses aligned stores
and does not call the memory primitives until translation is enabled.
[string.c](../lib/string.c) operates on Normal RAM with `SCTLR_EL1.A` cleared:
`memset`, `memcpy`, and `memmove` use 64-bit accesses at any byte offset, then
copy the remaining bytes without widening past the requested range.

### Identity Mappings

[mmu.c](../mm/mmu.c): a 39-bit TTBR0 address space with three levels (L1/L2/L3), 
512 entries per table, and 4 KiB pages. There are no block descriptors. 
The static table pages live in page-aligned BSS, are zeroed by startup,
and remain reserved below `_end`. No allocator is needed to build them, 
and the buddy allocator cannot reclaim them. TTBR1 is zero and its walks are 
disabled with `TCR_EL1.EPD1`.

| Mapping | Attribute | Access |
| --- | --- | --- |
| All 128 MiB RAM | Normal non-cacheable, MAIR index 1 (`0x44`) | EL1 read/write/execute |
| Three complete app slots | Same RAM attribute | Also EL0 read/write/execute, shared by all apps |
| PL011, 4 KiB | Device-nGnRnE, MAIR index 0 (`0x00`) | EL1 read/write, PXN and UXN |
| GIC distributor, 64 KiB | Same Device attribute | EL1 read/write, PXN and UXN |
| CPU 0 redistributor and SGI frame, 128 KiB | Same Device attribute | EL1 read/write, PXN and UXN |

All other addresses remain unmapped. RAM descriptors request Inner Shareable;
Normal non-cacheable and Device memory have effective Outer Shareable semantics,
which is what `AT` reports in `PAR_EL1`. Table walks are non-cacheable. Startup
publishes table stores with `DSB SY`, programs MAIR/TCR/TTBR, executes
`ISB; TLBI VMALLE1; DSB SY; ISB`, then sets `SCTLR_EL1.M` followed by `ISB`.
`A`, `C`, `I`, and `WXN` are explicitly cleared; stack alignment checks are
preserved. PC, SP, vectors, and the return address retain
their values across the switch because their mappings are identical.

The address equality is temporary: `page_to_phys()` still returns a physical
address. High-address kernel mappings and explicit physical/pointer conversions
belong to the next milestone, followed by permission hardening and cache enablement.

## Follow the Execution

1. [boot.S](../kernel/boot.S) masks exceptions, selects the EL1 stack, turns off
   MMU/caches, clears BSS, and installs the 2 KiB-aligned vector table. It then
   calls `mmu_init()` to enable identity translation while keeping caches off.
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

EL0 prohibits privileged instructions and cannot access kernel or device pages,
but all app slots share one mapping: there is no app-to-app isolation, no guard
pages within the slots, and no stack execute protection. Only trusted apps are supported.
Applications get no memory system calls; the allocators serve the kernel only.
All code is compiled with `-mgeneral-regs-only`; floating point, SIMD and SVE
contexts are deliberately unsupported. RAM remains writable and executable;
ELF segment flags do not yet determine page permissions. Caches stay off to keep
copied-code startup simple.

[test_kernel.py](../tests/test_kernel.py) builds real fixtures and captures bounded
QEMU serial output. Three no-SVC loops run both without a debugger and with
external GDB observations of nine timer exceptions. These check timer-driven
progress, task order and register/flag preservation over repeated resumes;
alternating demo log lines alone do not prove preemption.
Other checks exercise syscall returns, faults, linker rejection, incremental
repackaging, and all runnable-task combinations. All test-only code lives in
`tests/`. Separate test images reuse the normal kernel object files, using
link-time entry wrapping, without test switches or hooks in production sources.
The normal demo image is still built and tested independently.

The external boot regression dirties BSS and sets the alignment-check bit
before the real initialization code runs. The memory test image walks tables
from `TTBR0_EL1`, without exposing private MMU symbols, and checks every table's
alignment, uniqueness, and reservation. It uses `AT S1E1R/W` and `AT S1E0R/W`
on every RAM page and both device table spans, verifying physical addresses,
effective memory attributes, user access and holes, including out-of-range and
high addresses. The same image runs string and allocator assertions; separate
EL1 entries exercise faults and spurious interrupt handling.
Register observations confirm the translation geometry and cache-off state;
allocator self-tests, timer preemption and actual EL0 execution remain separate
behavioral checks. `AT` does not test instruction fetch: device execute-never bits
are inspected in the live descriptors; real execute-fault tests belong to the
permission-hardening milestone.