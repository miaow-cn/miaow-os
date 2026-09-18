<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Third-Party Inventory

One entry per imported component, as required by
[development.md](development.md#licensing).

## lib/vsprintf.c

- Scope: [lib/vsprintf.c](../lib/vsprintf.c) only. The declarations in
  [sprintf.h](../include/miaow/sprintf.h) and the printk wrapper are original
  project code under GPL-3.0-or-later.
- Upstream: Linux kernel, `lib/vsprintf.c`.
- Revision: Linux 2.4.8 (August 2001), kernel.org v2.4 release series.
- Original license: GPL-2.0-only. The file carries no explicit notice, so the
  kernel's COPYING applies, and the kernel is version 2 only. Copyright
  (C) 1991, 1992 Linus Torvalds; the historical Wirzenius and Dunnavant
  comments are preserved. License text: [GPL-2.0-only.txt](../LICENSES/GPL-2.0-only.txt).
- Modifications: kernel-internal includes replaced by `<stddef.h>` and
  `<miaow/sprintf.h>`; local ctype/strnlen shims added; `do_div()` replaced by
  plain 64-bit division; C23 `[[fallthrough]]` attributes added; digit tables
  moved to file scope because unaligned rodata-to-stack copies fault while the
  MMU is off. The formatting algorithm is unchanged.
- Compatibility decision: GPL-2.0-only cannot be combined with the project's
  GPL-3.0-or-later code in a distributed work. This non-distributed learning
  project does not trigger redistribution obligations, so the file is kept
  under its original license and notices and is not relabeled as project
  code. Before any distribution, replace it with a clean-room or
  GPL-2.0-or-later-compatible implementation.
