<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Development

Supreme rule: Keep all descriptions/comments/statements/requirements/commitmessage/.. compact unless explicitly requested.

## Requirements

[requirements.toml](requirements.toml) is the single register (parssed by Python `tomllib`). Each entry has exactly:

| Field | Contract |
| --- | --- |
| `id` | Unique `REQ-AREA-NNN`; uppercase alphanumeric area starting with a letter, three digits |
| `title` | Nonblank short label |
| `statement` | Nonblank observable behavior, normally "The ... shall ..." |
| `acceptance` | Nonempty list of nonblank, measurable criteria |
| `status` | `planned`, `implemented`, or `retired` |

Add requirements before implementation: `planned` until code and acceptance tests exist, then `implemented`, then run the gate. IDs are permanent — keep retired entries, never renumber/reuse; use Git history, not a change log. Evolving a contract updates its entry; distinct behavior gets a new ID; bug fixes reuse the original requirement. Traceability is one-way: tests name the requirements they verify. 

## Tests and Evidence

Use stdlib `unittest` in `tests/test_*.py` with metadata on each method. The fully qualified method ID is the test ID; avoid renames. Example for a future approved requirement (illustrative, not runnable):

```python
from tests.support import verifies

@verifies("REQ-BOOT-001")
def test_boot_banner(self):
    self.assertEqual(observed_banner, expected_banner)
```

Tests must exercise real behavior. Tests↔requirements may be many-to-many. Every discovered test needs valid, non-retired links; every implemented requirement needs tests. 

```sh
.venv/bin/python -m unittest tests.test_workflow.RegisterTests -v
.venv/bin/python tools/check.py
```

The first command is a focused example, not a substitute for the gate. The gate runs the validated suite once and writes `build/test-results.json`.

An implemented requirement passes only when all linked tests pass.

Do not add test switches, assertion blocks to `kernel/`, `mm/`, and `lib/`. Test code should be kept in `tests/`, use GNU ld `--wrap=start_kernel` when needed. See `tests/CMakeLists.txt` for details. Example:

```sh
cmake --build build --target memory_test_image
```

`tests/gdb_checks.py` runs inside GDB, not unittest discovery. The Python runner starts QEMU paused on a temporary localhost GDB port, gives the debugger a 20-second deadline, and terminates/reaps QEMU on success, error, or timeout.

## Completion and Growth

Complete = requirements, implementation, linked tests, and affected docs agree; focused tests and full gate pass; licensing reviewed. No auto-commit, no reformatting unrelated code.

## Debugging

```sh
qemu-system-aarch64 <run-target-flags> -s -S &
gdb build/kernel.elf -ex 'target remote :1234'
```

The CPU halts at QEMU's entry stub at 0x40000000, which hands the DTB pointer to the kernel image at 0x40080000; the launch configuration breaks on that physical entry. Symbols are linked at high addresses, so code before `__mmap_switched` (boot.S and `mmu_init`) runs at physical aliases: subtract `0xffffffbfc0000000` from a symbol address to break there. Symbolic breakpoints work from `__mmap_switched` on. Debug the kernel itself; apps are stripped raw binaries at fixed addresses with no symbol file.

`.venv/bin/python tools/get.py item1 [item2 [item3 [...]]]` prints system or cpu registers and memory words from a bare reset machine, which answers "what does the hardware default to?" without a build. See the script's docstring for details.

## Licensing

Original code, docs, tests, and config use `GPL-3.0-or-later`. Add format-appropriate SPDX license/copyright comments with the current year and local Git identity; use adjacent `.license` sidecars for files that cannot contain comments. Canonical license texts are unmodified and need no project copyright header. Full license text lives under `LICENSES/`.

Before the first third-party import, record in [third-party.md](third-party.md): file scope, upstream URL, exact revision/version, original license, modifications, and the compatibility decision. Preserve upstream notices and required license texts. Any license may be evaluated, but not all combine into a GPL-distributed work — use a compatible alternative or legally appropriate separation when needed.