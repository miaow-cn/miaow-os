<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Agent Instructions

Supreme rule: Keep all descriptions/comments/statements/requirements/commitmessage/.. compact unless explicitly requested.

## Scope
- This is a solo learning OS for QEMU AArch64, not a production system.
- Use English for repository code, comments, requirements, docs, tests, diagnostics, and commit messages. 
- Baseline: C23, freestanding, minimal AArch64 assembly. 

## Change Workflow
1. Read relevant entries in [docs/requirsements.toml](docs/requirements.toml) and nearby code/tests. 
2. Before implementation, add or update measurable requirements and acceptance criteria. Do not invent requirements for editorial-only changes.
3. Make a small coherent change, add linked regression tests, and update affected documentation in the same task. 
4. Run the focused test first, then `.venv/bin/python tools/check.py` before declaring completion. Missing tools, skips, and unexecuted checks are blockers. Review assertions against acceptance criteria, not just IDs.
5. Report requirement IDs, actual verification and blockers, plus a short explanation of the relevant OS concept or design tradeoff. 

See [docs/development.md](docs/development.md) for schema and test conventions. Reuse an existing requirement for a bug fix to unchanged behavior. Keep IDs stable; retain retired entries and never reuse their IDs. Do not maintain a second matrix.

## Linux Reference
- The Linux at `~/linux` and its Git history are the primary design reference. Consult them before any design. Prefer a smaller and clearer revision that already contains the algorithm. tips: use `git log --follow`.
- Port rather than reinvent: copy the upstream code, then delete everything this project does not need. Write original code only when the upstream file is unusable for license reasons, or when so little would survive the simplification that nothing recognizable remains.
- Simplify to learning scale: single CPU,  no NUMA, zones.
- Check the upstream `SPDX-License-Identifier` before copying. `GPL-2.0-or-later` may be relicensed to `GPL-3.0-or-later`; `GPL-2.0-only` keeps its original notices. Record the upstream path and exact commit or tag in [docs/third-party.md](docs/third-party.md).

## Environment
- Python: use `.venv` for environments and dependencies; prefer the standard library.
- Do not install system tools or other language environments. Identify missing prerequisites and give the user installation instructions appropriate to their OS.

## Git
- Commit, branch, and push only when explicitly requested. 
- Use concise imperative subjects: `subsystem: description`, at most 72 characters, without a final period.
- Don't add AI models or agents to `signed-off-by`, `co-authored-by` or `assisted-by`. Only human can take response, and the `assisted-by` tags bring no benefit to development.

## Licensing and Context
- Original files use `SPDX-License-Identifier: GPL-3.0-or-later` and `SPDX-FileCopyrightText: YEAR NAME <EMAIL>` using the current year and `git config --local user.name` / `git config --local user.email`. Ask if identity is missing; preserve existing notices and earlier years.
- Preserve third-party ownership and license notices; record source, revision, modifications, and compatibility before incorporating code. See development docs.
- This file is the single project policy source. Read detailed docs only as needed. Do not add duplicate instruction files, custom agents, skills, or hooks unless a recurring workflow justifies their maintenance and context cost.