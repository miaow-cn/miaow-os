<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Development

## Requirements

[requirements.toml](requirements.toml) is the single register (parsed by Python `tomllib`). Schema v1 accepts only `schema_version` and `requirements` at top level; each entry has exactly:

| Field | Contract |
| --- | --- |
| `id` | Unique `REQ-AREA-NNN`; uppercase alphanumeric area starting with a letter, three digits |
| `title` | Nonblank short label |
| `statement` | Nonblank observable behavior, normally "The ... shall ..." |
| `acceptance` | Nonempty list of nonblank, measurable criteria |
| `status` | `planned`, `implemented`, or `retired` |

Add requirements before implementation: `planned` until code and acceptance tests exist, then `implemented`, then run the gate. IDs are permanent — keep retired entries, never renumber/reuse; use Git history, not a change log. Evolving a contract updates its entry; distinct behavior gets a new ID; bug fixes reuse the original requirement. Traceability is one-way: tests name the requirements they verify; requirements never name tests, fixtures, or verification methods. Write criteria as observable behavior, not suite work. Language, authorship, and Git policies live in [AGENTS.md](../AGENTS.md), not in requirements. Status and passing evidence are separate: rerun the gate after changes toward the first OS milestone.

## Tests and Evidence

Use stdlib `unittest` in `tests/test_*.py` with metadata on each method. The fully qualified method ID is the test ID; avoid renames. Example for a future approved requirement (illustrative, not runnable):

```python
from tests.support import verifies

@verifies("REQ-BOOT-001")
def test_boot_banner(self):
    self.assertEqual(observed_banner, expected_banner)
```

Tests must exercise real behavior; inspecting source text or merely attaching an ID is not acceptance. Tests↔requirements may be many-to-many. Every discovered test needs valid, non-retired links; every implemented requirement needs tests. Discovery/import failures and an empty suite fail the gate. Discovery imports modules but does not execute test methods; keep import-time code free of external side effects.

```sh
.venv/bin/python -m unittest tests.test_workflow.RegisterTests -v
.venv/bin/python tools/check.py
```

The first command is a focused example, not a substitute for the gate. The gate runs the validated suite once and writes `build/test-results.json`: UTC generation time, overall pass/fail, structural/fixture errors; each test's requirement IDs, outcome, and failure/skip diagnostics; each requirement's state, linked test IDs, and verification result.

An implemented requirement passes only when all linked tests pass. Failures, errors, and unexpected successes fail acceptance; skips, expected failures, and unexecuted tests are incomplete. Failing/skipped subtests cannot be overwritten by successful siblings. Fixture errors fail the gate and make otherwise-passing requirements incomplete. Planned requirements stay pending even if tests pass; retired stay retired. Any non-passing executed test fails the gate, even one linked only to planned requirements.

The old report is deleted before validation; structural errors yield a failing report when possible, interruption yields none — never valid old evidence. Reports describe one run: rerun after changes; do not commit them or maintain a separate matrix. Tests do not prove their own adequacy: review assertions against every acceptance criterion. Manual verification needs explicit agreement; never silently substitute it for tests.

Tool regressions use temporary fixture projects and subprocesses to check failure exit codes without recursively running the project suite. Kernel tests build into temporary directories, run QEMU with a ten-second deadline, capture serial diagnostics, and terminate/reap emulators even on failure. Add host unit tests for hardware-independent logic; host success does not prove target behavior.

Production `kernel/`, `mm/`, and `lib/` sources have no test-only switches, assertion blocks, or self-test entry points. `tests/CMakeLists.txt` defines three explicit, non-default targets:  memory_test_image`, `fault_test_image`, and `timer_test_image`. For example:

```sh
cmake --build build --target memory_test_image
```

Each test image links the same `kernel_core`, `kernel_lib`, and `kernel_mm` object files as the normal image. GNU ld `--wrap=kernel_main` selects a test entry without changing production sources; the memory fixture also wraps `mem_init` to run assertions after the real initializer returns. Normal builds do not compile the fixtures. A regression verifies that building all test images preserves production objects and the normal image byte-for-byte.

`tests/gdb_checks.py` runs inside GDB, not unittest discovery. The Python runner starts QEMU paused on a temporary localhost GDB port, gives the debugger a 20-second deadline, and terminates/reaps QEMU on success, error, or timeout. GDB must support AArch64 and embedded Python; set `GDB=gdb-multiarch` where needed. Missing GDB is a failed prerequisite, never a skipped test.

The boot check dirties BSS externally before `_start`, checks the entire cleared range before `mmu_init` (except the saved DTB pointer), and sets `SCTLR.A` before verifying that initialization clears it. QEMU exposes `SCTLR` read-only through GDB, so the script executes `MSR SCTLR_EL1, x0; ISB; BR x1` from temporary firmware RAM at `0x40070000`, then restores those bytes and the general registers. The normal boot image contains no injection code. Scheduler checks stop at actual `trap`/`enter_app` instruction boundaries and inspect contexts and task state, without replacing the scheduler or adding trace calls. Debugger stops affect timing: these assertions prove ordering and state preservation, not wall-clock latency. Separate no-debugger runs verify actual timer-driven progress of all three no-SVC loops and completion of the normal demos.

## Completion and Growth

Complete = requirements, implementation, linked tests, and affected docs agree; focused tests and full gate pass; licensing reviewed. Report unverified items explicitly; keep explanations short and tied to the change. No auto-commit, no reformatting unrelated code, no placeholder subsystems, no CI/container infrastructure until needed; a future CI job should reuse the local gate.

First boot target: QEMU `virt`, Cortex-A710 (Armv9-A), GICv3, 1 CPU, 128 MiB RAM, headless serial, virtualization/security extensions off. Kernel at EL1, apps at EL0; startup enables identity-mapped MMU translation before entering C kernel main, with caches kept off. Document with the boot test: verified versioned machine, device addresses, entry exception level, load address, working tool versions. Fixed addresses are scoped to that tested platform, not a portability guarantee across QEMU versions. Confirm real `-std=c23` freestanding cross-compilation for Cortex-A710; never silently downgrade the standard or install another toolchain. An existing Linux cross compiler is acceptable with freestanding flags and no hosted link inputs.

The [first milestone design](../README.md#first-os-milestone) explains the execution path and exclusions. Add code in boot → EL0/syscall → preemption increments, testing each first. Keep new requirements planned until implementation and linked behavioral tests exist. Missing QEMU or compilers blocks acceptance; source-text tests or skips are no substitute for execution.

Run `.venv/bin/python -m unittest tests.test_kernel -v` for the OS slice, then the full gate. The host scheduler harness exhausts all three-task runnable combinations. QEMU fixtures and external debugger checks cover no-SVC preemption with integer register patterns, syscall boundaries, EL0/EL1 faults, spurious interrupts, and task termination. The BSS check does not rely on QEMU's zeroed RAM. Shared string assertions have separate host and target entry files, not compile-time branches in the implementation.

## Debugging

VS Code F5 ("QEMU kernel debug") builds, starts QEMU with `-s -S` (GDB stub on TCP 1234, CPU halted at reset), and connects with symbols from `build/kernel.elf`. QEMU machine flags in `.vscode/tasks.json` mirror the CMake `run` target — keep them in sync. Host prerequisites: C/C++ extension and an AArch64-capable GDB (Fedora's `gdb` is built with `--enable-targets=aarch64-linux-gnu,...`; Debian/Ubuntu need `sudo apt install gdb-multiarch`). Stopping the session kills QEMU. Without VS Code:

```sh
qemu-system-aarch64 <run-target-flags> -s -S &
gdb build/kernel.elf -ex 'target remote :1234'
```

The CPU halts at QEMU's entry stub at 0x40000000, which hands the DTB pointer to the kernel image at 0x40080000; break on the kernel entry and continue to skip it. Debug the kernel itself; apps are stripped raw binaries at fixed addresses with no symbol file.

## Licensing

Original code, docs, tests, and config use `GPL-3.0-or-later`. Add format-appropriate SPDX license/copyright comments with the current year and local Git identity; use adjacent `.license` sidecars for files that cannot contain comments. Canonical license texts are unmodified and need no project copyright header; generated local reports are not authored source. Full GPLv3 text lives under `LICENSES/`.

Before the first third-party import, record in [third-party.md](third-party.md): file scope, upstream URL, exact revision/version, original license, modifications, and the compatibility decision. Preserve upstream notices and required license texts; do not relabel upstream code as original GPL. Any license may be evaluated, but not all combine into a GPL-distributed work — use a compatible alternative or legally appropriate separation when needed.