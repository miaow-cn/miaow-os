<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Third-Party Inventory

One entry per imported component, as required by [development.md](development.md#licensing).

## MMU Design References (No Imported Code)

- Scope: [mmu.c](../mm/mmu.c) and its call from [boot.S](../kernel/boot.S).
- The earliest ARM64 boot implementation is `arch/arm64/kernel/head.S` at `9703d9d7f77ce129621f7d80a844822e2daa7008`. Register setup and descriptor definitions were checked against `arch/arm64/mm/proc.S` and `arch/arm64/include/asm/pgtable-hwdef.h` at tag `v3.7` in the Linux kernel.
- These historical files predate SPDX tags and explicitly license their code under GPL version 2 only. No code or notices from them are incorporated.
- The fixed three-level, page-only RAM/device tables need none of Linux's block mapping, relocation, high-address transition, CPU selection, SMP, or cache maintenance implementation. Too little remains for a recognizable port; this is original GPL-3.0-or-later code using architecture-defined descriptors and the publish/configure/TLBI/enable sequence. It introduces no additional GPL-2.0-only distribution dependency.

## String Alignment References (No Imported Code)

- Scope: [string.c](../lib/string.c) and the alignment control in [mmu.c](../mm/mmu.c).
- Reference: Linux commit `518e5b794c06c0f0eb40df3e202274a66202c137`, `arch/arm64/lib/memcpy.S`, `arch/arm64/include/asm/sysreg.h`, and `include/linux/unaligned/packed_struct.h`. ARM64 memcpy explicitly supports unaligned accesses; its large-copy loop still aligns the destination for performance. Linux's C helpers express unaligned accesses through packed types.
- The assembly has SPDX `GPL-2.0-only`; the packed helper has no file-level license notice and is covered by Linux's GPL-2.0-only license. No code is copied. The existing project loops are retained with a GCC `aligned(1), may_alias` typedef instead of importing assembly, pipelining, or a generic helper family. Too little of that machinery would survive the learning-scale simplification for a recognizable port; the code remains original GPL-3.0-or-later.

## lib/vsprintf.c

- Scope: [lib/vsprintf.c](../lib/vsprintf.c) only. The declarations in [sprintf.h](../include/miaow/sprintf.h) and the printk wrapper are original project code under GPL-3.0-or-later.
- Upstream: Linux kernel, `lib/vsprintf.c`.
- Revision: Linux 2.4.8 (August 2001), kernel.org v2.4 release series.
- Original license: GPL-2.0-only. The file carries no explicit notice, so the kernel's COPYING applies, and the kernel is version 2 only. Copyright (C) 1991, 1992 Linus Torvalds; the historical Wirzenius and Dunnavant comments are preserved. License text: [GPL-2.0-only.txt](../LICENSES/GPL-2.0-only.txt).
- Modifications: kernel-internal includes replaced by `<stddef.h>` and `<miaow/sprintf.h>`; local ctype/strnlen shims added; `do_div()` replaced by plain 64-bit division; C23 `[[fallthrough]]` attributes added; digit tables moved to file scope because unaligned rodata-to-stack copies fault while the MMU is off. The formatting algorithm is unchanged.
- Compatibility decision: GPL-2.0-only cannot be combined with the project's GPL-3.0-or-later code in a distributed work. This non-distributed learning project does not trigger redistribution obligations, so the file is kept under its original license and notices and is not relabeled as project code. Before any distribution, replace it with a clean-room or GPL-2.0-or-later-compatible implementation.

## include/miaow/list.h

- Scope: [list.h](../include/miaow/list.h) only.
- Upstream: Linux kernel, `include/linux/list.h` and `include/linux/container_of.h`.
- Revision: `518e5b794c06c0f0eb40df3e202274a66202c137`, described as `v7.3-rc3-520-g518e5b794c06`.
- Original license: GPL-2.0-only, declared by the upstream `SPDX-License-Identifier: GPL-2.0` tag. License text: [GPL-2.0-only.txt](../LICENSES/GPL-2.0-only.txt).
- Modifications: reduced to `container_of`, `LIST_HEAD`, `INIT_LIST_HEAD`, `list_add`, `list_add_tail`, `list_del`, `list_empty`, `list_entry`, `list_first_entry`, `list_next_entry` and `list_for_each_entry`; `READ_ONCE`/`WRITE_ONCE` dropped because this kernel is single-CPU and never touches a list from an interrupt; list validation, pointer poisoning, the `container_of` type check and the RCU, hlist, safe, reverse and `*_careful` variants removed. The algorithm is unchanged.
- Compatibility decision: same as `lib/vsprintf.c` above.

## mm/memblock.c and include/miaow/memblock.h

- Scope: [memblock.c](../mm/memblock.c) and [memblock.h](../include/miaow/memblock.h).
- Upstream: Linux kernel, `mm/memblock.c` and `include/linux/memblock.h`.
- Revision: `518e5b794c06c0f0eb40df3e202274a66202c137`, described as `v7.3-rc3-520-g518e5b794c06`. Copyright (C) 2001 Peter Bergner, IBM Corp.
- Original license: GPL-2.0-or-later.
- Modifications: NUMA node ids, region flags, the resizable region arrays, the top-down search, kmemleak and debugfs removed; the region arrays are fixed-size statics and insertion fails instead of growing them; allocation is bottom-up only and returns uninitialized memory. The region bookkeeping algorithm, including `memblock_add_range`, `memblock_isolate_range`, `memblock_merge_regions` and the free-range walk, is unchanged.
- Compatibility decision: GPL-2.0-or-later permits use under GPL-3.0-or-later, so these files carry the project license with the upstream copyright preserved.

## mm/page_alloc.c and include/miaow/mm.h

- Scope: [page_alloc.c](../mm/page_alloc.c) and [mm.h](../include/miaow/mm.h). `mm/init.c` is original project code under GPL-3.0-or-later.
- Upstream: Linux kernel, `mm/page_alloc.c`.
- Revision: Linux v2.6.12, chosen because it predates migrate types and is far closer to the plain buddy algorithm than current code. Copyright (C) 1991, 1992, 1993, 1994 Linus Torvalds; buddy commentary by William Lee Irwin III.
- Original license: GPL-2.0-only. The v2.6.12 file carries no explicit notice, so the kernel's COPYING applies and that kernel is version 2 only. License text: [GPL-2.0-only.txt](../LICENSES/GPL-2.0-only.txt).
- Modifications: one implicit zone instead of pgdat and zonelists; no GFP flags, watermarks, per-CPU page lists, reclaim, compound pages, page reference counts or bootmem interoperation; free block state lives in explicit `struct page` fields instead of `PG_private` and `page->private`; buddies are computed from the absolute page frame number rather than the `MAX_ORDER`-masked index, so the managed range need not be `MAX_ORDER` aligned. `__free_one_page`, `expand` and the `__rmqueue` search are unchanged.
- Compatibility decision: same as `lib/vsprintf.c` above.

## mm/slab.c and include/miaow/slab.h

- Scope: [slab.c](../mm/slab.c) and [slab.h](../include/miaow/slab.h).
- Upstream: Linux kernel, `mm/slub.c`.
- Revision: commit `81819f0fc8285a2a5a921c019e3e3d7b6169d225` ("SLUB core", Linux 2.6.22), the commit that introduced SLUB and the smallest form of it. Copyright (C) 2007 SGI, Christoph Lameter.
- Original license: GPL-2.0-only. The file carries no explicit notice, so the kernel's COPYING applies. License text: [GPL-2.0-only.txt](../LICENSES/GPL-2.0-only.txt).
- Modifications: no per-CPU slab, locking, NUMA nodes, debug or poisoning options, sysfs, cache merging or cache creation by name; one page per slab; the kmalloc size classes are the only caches; the free pointer sits at offset zero inside a free object instead of at a configurable offset. The freelist handling and the movement of slabs between the partial list and the page allocator follow the original.
- Compatibility decision: same as `lib/vsprintf.c` above.
