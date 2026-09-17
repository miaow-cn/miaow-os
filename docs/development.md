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
artificial software requirements. No OS requirements are approved in advance.

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
exit codes without recursively executing the project suite. Future QEMU tests should
use this same runner, bound execution with timeouts, capture serial diagnostics,
and clean up their emulator processes. Add host unit tests for hardware-independent
logic when introduced; do not assume host success proves target behavior.

## Completion and Growth

A change is complete when requirements, implementation, linked tests, and affected
docs agree, the focused tests and full gate pass, and licensing has been reviewed.
Report unverified items explicitly. Keep explanations short and tied to the change.
Do not auto-commit, reformat unrelated code, add placeholder subsystems, or introduce
CI/container infrastructure until needed. A future CI job should reuse the local gate.

First boot defaults: QEMU `virt`, AArch64, one CPU, headless serial. Choose and
document the CPU model, boot entry/exception level, load address, and working tool
versions with that requirement. Confirm real `-std=c23` freestanding cross-compilation
support; do not silently use an older standard or install another toolchain.

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