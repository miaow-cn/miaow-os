<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Development

## Requirements

[requirements.toml](requirements.toml) is the single register; Python `tomllib`
parses it. Schema version 1 accepts only `schema_version` and `requirements` at
the top level. Each entry has exactly these fields:

| Field | Contract |
| --- | --- |
| `id` | Unique `REQ-AREA-NNN`; uppercase alphanumeric area starting with a letter, three digits |
| `title` | Nonblank short label |
| `statement` | Nonblank observable behavior, normally "The ... shall ..." |
| `acceptance` | Nonempty list of nonblank, measurable criteria |
| `status` | `planned`, `implemented`, or `retired` |

Add requirements before implementation. Use `planned` until code and acceptance
tests exist, then set `implemented` and run the gate. IDs are permanent: keep
retired entries, never renumber/reuse them, and use Git history for changes rather
than a separate change log. Evolving the same contract may update its entry;
distinct behavior gets a new ID. A bug fix normally reuses its original requirement.
Language, authorship, and Git policies live in [AGENTS.md](../AGENTS.md), not in
artificial software requirements. Implementation status and current passing evidence
are separate: rerun the gate after changes to the first OS milestone.

## Tests and Evidence

Use standard-library `unittest` in `tests/test_*.py`, with metadata on each test
method. A fully qualified unittest method ID is the test ID; avoid unnecessary
renames. Example for a future approved requirement:

```python
from tests.support import verifies

@verifies("REQ-BOOT-001")
def test_boot_banner(self):
    self.assertEqual(observed_banner, expected_banner)
```

The example is not a boot implementation or a runnable test. Tests must exercise
real behavior; inspecting source text or merely attaching an ID is not acceptance.
One test may reference several requirements; one requirement may have several tests.
Every discovered test needs valid, non-retired links; every implemented requirement
needs tests. Discovery/import failures and an empty suite fail the gate. Discovery
imports modules, but link validation does not execute test methods; keep import-time
code free of external side effects.

```sh
.venv/bin/python -m unittest tests.test_workflow.RegisterTests -v
.venv/bin/python tools/check.py
```

The first command is a focused example, not a substitute for the second. The gate
runs the validated suite once and writes `build/test-results.json`, containing:
- UTC generation time, overall pass/fail, and structural or fixture errors.
- Each test's requirement IDs, actual outcome, and failure/skip diagnostics.
- Each requirement's implementation state, linked test IDs, and verification result.

An implemented requirement passes only when all its linked tests pass. Failures,
errors, and unexpected successes fail acceptance; skips, expected failures, and
unexecuted tests are incomplete. Failing/skipped subtests cannot be overwritten by
successful siblings. Fixture errors fail the gate and conservatively make otherwise
passing requirements incomplete. Planned requirements stay pending even if tests
pass; retired ones stay retired. Any non-passing executed test fails the overall gate,
including a test linked only to planned requirements.

The old report is deleted before validation. Structural errors produce a failing
report when possible; interruption may leave no report, never valid old evidence.
Reports describe one run, not future edits: rerun after changes. Do not commit them
or maintain a separate traceability matrix. Tests do not prove their own adequacy:
review assertions against every acceptance criterion. Manual verification support
must be agreed explicitly before use; do not silently substitute it for tests.

Tool regressions use temporary fixture projects and subprocesses to check failure
exit codes without recursively executing the project suite. Kernel tests build into
temporary directories, run QEMU with a ten-second deadline, capture serial diagnostics,
and terminate/reap their emulator processes even on failure. Add host unit tests for hardware-independent
logic when introduced; do not assume host success proves target behavior.

## Completion and Growth

A change is complete when requirements, implementation, linked tests, and affected
docs agree, the focused tests and full gate pass, and licensing has been reviewed.
Report unverified items explicitly. Keep explanations short and tied to the change.
Do not auto-commit, reformat unrelated code, add placeholder subsystems, or introduce
CI/container infrastructure until needed. A future CI job should reuse the local gate.

First boot target: QEMU `virt`, Cortex-A710 (Armv9-A), GICv3, one CPU, 128 MiB RAM,
headless serial, and virtualization/security extensions disabled. The kernel runs
at EL1, applications at EL0, initially with MMU and caches off. Document the
verified versioned machine, device addresses, entry exception level, load address,
and working tool versions with the boot test. Fixed device addresses are scoped
to that tested platform, not a portability guarantee across QEMU versions.
Confirm real `-std=c23` freestanding cross-compilation for Cortex-A710; do not
silently use an older standard or install another toolchain. An existing Linux
cross compiler is acceptable with freestanding flags and no hosted link inputs.

The [first milestone design](../README.md#first-os-milestone) explains the small
execution path and exclusions. Add code in boot, EL0/syscall, and preemption
increments, testing each before adding the next. Keep new requirements planned
until their implementations and linked behavioral tests exist. Missing QEMU or
compilers blocks acceptance; no source-text tests or skips substitute for execution.

Run `.venv/bin/python -m unittest tests.test_kernel -v` for the OS slice, then the
full gate. The host scheduler harness exhausts all three-task runnable combinations.
QEMU fixtures check no-SVC preemption with integer register patterns, syscall
boundaries, EL0/EL1 faults, spurious interrupts, and task termination. Test-only
compile flags enable narrow observations in the real handler, not a second scheduler.
Normal builds omit them. The BSS test dirties BSS before startup clears it, rather
than relying on QEMU's initially zero RAM.

## Licensing

Original code, docs, tests, and config use `GPL-3.0-or-later`. Add format-appropriate
SPDX license and copyright comments with the current year and local Git identity.
Use adjacent `.license` sidecars for files that cannot contain comments. Canonical
license texts are unmodified and need no project copyright header; generated local
reports are not authored source. The full GPLv3 text is stored under `LICENSES/`.

Before the first third-party import, add a concise inventory recording file scope,
upstream URL, exact revision/version, original license, modifications, and the
compatibility decision. Preserve upstream notices and provide required license
texts. Do not relabel upstream code as original GPL code. Any open-source project
may be evaluated, but not every license can be combined into a GPL-distributed
work; use a compatible alternative or a legally appropriate separation when needed.
The inventory lives in [third-party.md](third-party.md).