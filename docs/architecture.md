<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

This is one small execution path, not a general OS framework. The authoritative behavior and acceptance criteria remain in [requirements.toml](requirements.toml).

## Platform and Addresses

The tested command is provided by `cmake --build build --target run`:

```sh
qemu-system-aarch64 \
  -machine virt-10.1,gic-version=3,virtualization=off,secure=off,its=off \
  -cpu cortex-a710 -smp 1 -m 128M \
  -display none -serial stdio -monitor none -no-reboot \
  -kernel build/kernel.bin
```

The raw image enters at EL1 at physical `0x40080000`. Other entry levels are diagnosed and halted, not normalized by extra firmware code. The tested machine model is QEMU `virt-10.1`, without secure or virtualized execution.

| Region | Physical address / size | Kernel virtual address |
| --- | --- | --- |
| RAM | `0x40000000`, 128 MiB | `0xffffffc000000000` (linear map) |
| Kernel image | Loaded at `0x40080000` | Linked at `0xffffffc000080000` |
| Kernel stack | 16 KiB after kernel BSS | Linear map |
| App pages | Allocated from the buddy allocator | Linear map |
| PL011 | `0x09000000` | `0xffffff8009000000` |
| GICv3 distributor | `0x08000000` | `0xffffff8008000000` |
| CPU 0 redistributor / SGI frame | `0x080a0000` / `0x080b0000` | `0xffffff80080a0000` / `0xffffff80080b0000` |

Each app has a separate TTBR0 address space:

| Region | User virtual address | Size |
| --- | --- | --- |
| Image | `0x400000` | Up to 64 KiB |
| Stack | `0x7fffffc000`–`0x8000000000` | 16 KiB |

These addresses come from [abi.h](../include/abi.h); the kernel layout comes from [memory.h](../include/miaow/memory.h). Each app has its own ELF for debugging; only its flat code/constants binary is embedded and copied. Writable static storage, TLS, runtime initialization and oversized images fail at link time. There is no ELF parser, relocation loader, or filesystem.

## Memory Management

The MMU is enabled before entering `start_kernel()`, and the kernel then runs at high virtual addresses. The allocator layers follow Linux and are built in that order, each on top of the previous one.

1. [memblock.c](../mm/memblock.c) describes RAM before any allocator exists. It holds two sorted, merged region arrays: available memory and reserved memory. `mem_init()` adds all of RAM, then reserves the firmware area, kernel image up to `_end`, and device tree blob. Walking memory minus reserved yields the free ranges.
2. [page_alloc.c](../mm/page_alloc.c) is a buddy allocator over a flat `struct page` array indexed by page frame number, itself allocated from memblock. `alloc_pages(order)` takes the smallest sufficient block and returns the unused halves to lower orders; `__free_pages()` merges a block with the buddy that differs only in bit `order` of its page frame number, repeatedly, up to `MAX_ORDER - 1`.
3. [slab.c](../mm/slab.c) carves single pages into one power-of-two size class each, from 16 to 2048 bytes, chaining free objects through their own storage. `kmalloc` above 2048 bytes falls back to whole pages and records the order in the page, so `kfree` needs nothing but the pointer.

QEMU leaves the device tree pointer in `x0`; [boot.S](../kernel/boot.S) saves it before anything else and `mem_init()` reserves the blob after checking its magic. Nothing parses the tree yet, so RAM extent still comes from `memory.h`.

During early startup, before enabling the MMU, data accesses behave as Device type and wide accesses must be naturally aligned. Startup uses aligned stores and does not call the memory primitives until translation is enabled.
[string.c](../lib/string.c) operates on Normal RAM with `SCTLR_EL1.A` cleared: `memset`, `memcpy`, and `memmove` use 64-bit accesses at any byte offset, then copy the remaining bytes without widening past the requested range.

### Kernel Mappings

[mmu.c](../mm/mmu.c): 39-bit TTBR0 and TTBR1 ranges with three levels (L1/L2/L3), 512 entries per table, and 4 KiB pages. There are no block descriptors.
`REQ-MM-004` defines this translation layout and memory types, not cache policy.
`create_pgd_mapping()` walks from an L1 root and takes missing L2/L3 tables from a static pool; later calls overwrite earlier pages, which is how kernel segments tighten the linear map. The roots (`swapper_pg_dir`, `reserved_pg_dir`, `idmap_pg_dir`) and pool live in page-aligned BSS and remain reserved below `_end`.

| Mapping | Attribute | Access |
| --- | --- | --- |
| TTBR1: all 128 MiB RAM at `PAGE_OFFSET` | Normal RAM; cache policy below | `PAGE_KERNEL`: EL1 read/write |
| TTBR1: kernel text, `_text`..`_etext` | Same RAM attribute | `PAGE_KERNEL_ROX`: EL1 read/execute |
| TTBR1: kernel rodata, `__start_rodata`..`__end_rodata` | Same RAM attribute | `PAGE_KERNEL_RO`: EL1 read |
| TTBR1: PL011, 4 KiB, at `IO_OFFSET` + PA | Device-nGnRnE, MAIR index 0 (`0x00`) | `PROT_DEVICE_nGnRnE`: EL1 read/write |
| TTBR1: GIC distributor, 64 KiB | Same Device attribute | Same |
| TTBR1: CPU 0 redistributor and SGI frame, 128 KiB | Same Device attribute | Same |
| TTBR0: app image pages | Same RAM attribute | `PAGE_READONLY_EXEC`: EL0 read/execute, EL1 read |
| TTBR0: app stack pages | Same RAM attribute | `PAGE_SHARED`: EL0 and EL1 read/write |

Everything not listed as executable is execute-never; each TTBR0 maps only its own image and stack. User entries remain global (`nG = 0`) until ASIDs are introduced. Kernel text and rodata have no writable alias. App code is written through its EL1-only linear alias.

Like the first ARM64 Linux port, the kernel image lives inside the linear map:
it is linked at `PAGE_OFFSET + 0x80000` and loaded at `PHYS_OFFSET + 0x80000`, so one mapping serves both. [memory.h](../include/miaow/memory.h) makes the boundary explicit. `__pa()` and `__va()` convert between linear-map pointers and physical addresses; `page_address()` and `virt_to_page()` do the same for `struct page`. memblock, table descriptors, TTBRs, and device tree pointer are physical. The buddy system, slab objects, and `kmalloc` are virtual. The kernel writes app pages through the linear map. Syscalls read app buffers through their user addresses. Drivers use `IO_ADDRESS()`.

The switch follows Linux's `__enable_mmu`/`__turn_mmu_on`/`__mmap_switched`:

1. [boot.S](../kernel/boot.S) runs at physical addresses to set the stack, clear BSS, and save the DTB.
2. `mmu_init()` also runs at physical addresses. It builds TTBR1 and places the linear map's L1 entry at the `PHYS_OFFSET` index of `idmap_pg_dir`. Because both offsets are 1 GiB aligned, the same L2/L3 tables then map all RAM to itself. It programs MAIR/TCR/TTBR0 (the idmap)/TTBR1 and sets `SCTLR_EL1.M`. Cache setup follows the next section. `A` and `WXN` are explicitly cleared. Its return still executes from the identity map.
3. A `BR` enters the link-time address of `__mmap_switched` at high mapping. The stack and `VBAR_EL1` are reloaded with high addresses. `cpu_uninstall_idmap()` switches TTBR0 to the empty `reserved_pg_dir` and flushes the TLB before `start_kernel()`.

### Cache and TLB Synchronization

RAM pages and both table walks use Inner Shareable, inner/outer write-back, read/write-allocate attributes, with MAIR RAM index 1 set to `0xff`. `SCTLR_EL1.C` and `I` are set before `start_kernel()`. `AT` reports Inner Shareable for RAM in `PAR_EL1`; Device memory retains effective Outer Shareable semantics.

Before enabling caches, `IC IALLU; DSB SY; ISB` removes stale local instructions. After copying each app through its writable high alias, [cache.S](../mm/cache.S) cleans every intersecting D-cache line to PoU (`DC CVAU`), deriving the line size from `CTR_EL0.DminLine`. `DSB SY; IC IALLU; DSB SY; ISB` then synchronizes execution through the low alias. Whole-local-I-cache invalidation covers both virtual aliases; barriers alone cannot discard stale instructions. This deliberately favors simplicity over range-invalidation performance on one CPU.

`flush_tlb_all()` publishes table stores with `DSB ISHST`, invalidates local EL1/EL0 translations with `TLBI VMALLE1`, then completes with `DSB ISH; ISB`. Every application switch replaces TTBR0 and performs this synchronization.

The memory fixture exercises a live leaf replacement with break-before-make and real data reads. ASIDs and SMP shootdowns are not provided. GDB tests trace executed cache/TLB maintenance and cache-line coverage. `test_cache_configuration` checks the cache attributes, enable bits, and live leaf update; `test_application_cache_synchronization` traces startup maintenance and application copies. Boot and identity-mapping tests check execution level and translation independently of cache policy.

### Runtime Page Tables

[memory.c](../mm/memory.c) edits tables after boot (`REQ-MM-008`); shared types and protections are in [pgtable.h](../include/miaow/pgtable.h). Tables are zeroed buddy pages accessed through `__va()`; descriptors hold physical addresses. Each task owns an `mm_struct` with its own root (`REQ-VM-001`).

- `pgd_alloc()`/`pgd_free()`: root of a `struct mm_struct` (only `pgd` so far). `pgd_free()` frees the whole table tree but not mapped pages; the root must not be live.
- `pte_alloc()`: walks to the leaf, adding missing L2/L3 tables; null on allocation failure.
- `set_pte_at()`/`pte_clear()`: a valid leaf is cleared and flushed with `flush_tlb_page()` before it changes (break-before-make).
- `switch_mm()`: writes the root's physical address to TTBR0 and flushes all translations; TTBR1 stays unchanged.

## Follow the Execution

1. [boot.S](../kernel/boot.S) masks exceptions, selects the EL1 stack, turns off MMU/caches, and clears BSS at physical addresses. It then calls `mmu_init()`, jumps to the high kernel mapping, installs the 2 KiB-aligned vector table, and removes the identity map. Instruction and data caches are enabled.
2. [main.c](../kernel/main.c) prints actual CPU control state, then `mem_init()` ([init.c](../mm/init.c)) brings up the allocators. [task.c](../kernel/task.c) allocates separate roots, image pages and zeroed stacks, verifies each copy, synchronizes its instruction cache, and initializes PC, SP and EL0t PSTATE.
3. [timer.c](../kernel/timer.c) initializes the GICv3 distributor, wakes the redistributor, enables Group 1 timer PPI 30, and enables the system-register CPU interface. `CNTFRQ_EL0 / 100` supplies a nominal 10 ms quantum.
4. [vectors.S](../kernel/vectors.S) enters EL0 using `ERET`. On `SVC` or IRQ it saves every integer register before C code can clobber it, plus `SP_EL0`, `ELR_EL1` and `SPSR_EL1`. C/assembly offsets are checked in [context.h](../kernel/context.h).
5. `trap()` handles a syscall or acknowledges the timer. Scheduling selects a runnable task, switches TTBR0 and flushes the TLB. `ERET` restores that task's registers, PC and flags and resumes EL0 with IRQ enabled.

One EL1 stack is sufficient because handlers do not block and IRQ stays masked throughout kernel execution. Polling UART writes make each bounded log operation serial, but also delay preemption while the kernel runs. This is not hard real-time scheduling. Logging itself does not yield; timer IRQ, exit and fault can switch tasks.

A task that exits or faults is no longer runnable. If one remains it keeps running; if none remain, the timer is disabled and the kernel prints `ALL APPS DONE` and idles. A task fault reports ESR/FAR/ELR; a kernel exception reports the same and halts. QEMU must be stopped separately. FAR is meaningful only for aborts.

## System Calls

`SVC #0` uses `x8` for the call number, `x0`/`x1` for arguments and `x0` for the signed result. Other registers and condition flags survive a returning syscall.

| Number | Call | Result |
| --- | --- | --- |
| 0 | `log(pointer, length)` | Payload byte count, or negative error |
| 1 | `exit(status)` | Records status; never returns |

Logs accept 0 to 256 bytes from the current app's actual image or stack. Zero length never dereferences the pointer. The `[app N] ` prefix is not counted in the result. Bounds checks reject invalid or crossing ranges, including kernel and device addresses. A shared numerical user address resolves only through the caller's page table. Errors are `-14` (`EFAULT`), `-22` (`EINVAL`, excessive length), and `-38` (`ENOSYS`, unknown call).

[app.h](../apps/app.h) contains the small register-based wrapper and stack-based integer formatting. [start.S](../apps/start.S) calls `app_main()` and exits with its return value. There is no user libc or `printf` for applications. Kernel diagnostics go through `printk` ([printk.c](../lib/printk.c)) over the polling UART, while log syscall payloads stay byte-exact and bypass the formatter.

## Limits and Evidence

Pages are allocated eagerly and are not reclaimed on exit. There are no VMAs, demand faults, safe user-copy fixups, ASIDs, or memory system calls yet; only trusted apps are supported. All code is compiled with `-mgeneral-regs-only`; floating point, SIMD and SVE contexts are unsupported.

[test_kernel.py](../tests/test_kernel.py) builds real fixtures and captures bounded QEMU serial output. GDB walks all user tables, compares image bytes and zeroed stacks, checks distinct physical pages and unchanged kernel tables, and traces TTBR0 and TLB maintenance over nine switches. Real EL0 probes test other apps' physical pages and the former slots; logging the same VA in each app returns its own data. Other checks cover preemption, permissions, syscall errors, allocation failures, linker rejection, and repackaging. Test-only code stays in `tests/`; separate test images reuse production objects.

QEMU does not fully model hardware cache incoherence, so successful app execution alone is insufficient.