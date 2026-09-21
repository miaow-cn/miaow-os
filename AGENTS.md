<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Agent Instructions

## Scope
- This is a solo learning OS for QEMU AArch64, not a production system.
- Use English for repository code, comments, requirements, docs, tests, diagnostics,
  and commit messages. Discuss and explain concepts in the user's language.
- Kernel baseline: C23, freestanding, minimal AArch64 assembly. Verify compiler
  support when introducing kernel code; do not silently downgrade the standard.
- Implement only requested capabilities. Questions and planning are not requests
  to implement. Do not introduce speculative subsystems, frameworks, or processes.

## Change Workflow
1. Read relevant entries in [docs/requirements.toml](docs/requirements.toml) and
   nearby code/tests. Clarify consequential ambiguity; otherwise proceed locally.
2. Before implementation, add or update measurable requirements and acceptance
   criteria. A clear implementation request authorizes that scope without another
   approval ceremony. Do not invent requirements for editorial-only changes.
3. Make a small coherent change, add linked regression tests, and update affected
   documentation in the same task. Preserve unrelated work and formatting.
4. Run the focused test first, then `.venv/bin/python tools/check.py` before
   declaring completion. Missing tools, skips, and unexecuted checks are blockers,
   not passing evidence. Review assertions against acceptance criteria, not just IDs.
5. Report requirement IDs, actual verification and blockers, plus a short explanation
   of the relevant OS concept or design tradeoff. No separate tutorial by default.

See [docs/development.md](docs/development.md) for schema and test conventions.
Reuse an existing requirement for a bug fix to unchanged behavior. Keep IDs stable;
retain retired entries and never reuse their IDs. Do not maintain a second matrix.

## Linux Reference
- The Linux checkout at `~/linux` and its Git history are the primary design
  reference. Consult them before designing a subsystem. Prefer the earliest
  revision that already contains the algorithm, found with `git log --follow`;
  it is usually far smaller and clearer than current code.
- Port rather than reinvent: copy the upstream implementation, then delete
  everything this project does not need. Write original code only when the
  upstream file is unusable for license reasons, or when so little would survive
  the simplification that nothing recognizable remains. State which case applies.
- Always simplify to learning scale: single CPU, no locking, no NUMA, zones,
  or per-CPU caches, no debug or hardening options, and no configuration knobs.
- Check the upstream `SPDX-License-Identifier` before copying. `GPL-2.0-or-later`
  may be relicensed to `GPL-3.0-or-later`; `GPL-2.0-only` keeps its original
  notices. Record the upstream path and exact commit or tag of every port in
  [docs/third-party.md](docs/third-party.md).

## Environment and Git
- Python: use `.venv` for environments and dependencies; prefer the standard library.
- Do not install system tools or other language environments. Identify missing
  prerequisites and give the user installation instructions appropriate to their OS.
- Inspect Git status before editing. Do not change local identity or discard work.
- Commit, branch, and push only when explicitly requested. Use concise imperative
  subjects: `subsystem: description`, at most 72 characters, without a final period.
- Do not automatically reformat code or add dependencies without a concrete need.

## Licensing and Context
- Original files use `SPDX-License-Identifier: GPL-3.0-or-later` and
  `SPDX-FileCopyrightText: YEAR NAME <EMAIL>` using the current year and
  `git config --local user.name` / `git config --local user.email`.
  Ask if identity is missing; preserve existing notices and earlier years.
- Preserve third-party ownership and license notices; record source, revision,
  modifications, and compatibility before incorporating code. See development docs.
- This file is the single project policy source. Read detailed docs only as needed.
  Do not add duplicate instruction files, custom agents, skills, or hooks unless
  a recurring workflow justifies their maintenance and context cost.