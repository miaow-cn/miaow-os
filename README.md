<!-- SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# miaow-os

A solo operating-system learning project targeting QEMU AArch64. Development is
incremental and requirement-driven, with AI agents assisting implementation and
explanation. The kernel baseline is freestanding C23 with minimal AArch64 assembly.

**Current state:** development workflow and traceability tooling only. There is no
kernel, boot image, or QEMU execution yet. Passing checks verify tooling, not an OS.

## Quick Start

Requires Git and Python 3.11+ with `venv`; no third-party Python packages are needed.
Run from the repository root:

```sh
python3 -m venv .venv
.venv/bin/python tools/check.py
```

The check validates requirements and test links, executes tests, and writes the
current traceability report to `build/test-results.json`. Exit zero means the gate
passed; planned requirements can remain pending. `.venv` and `build` are ignored.
If Python or `venv` is unavailable, install it through your OS package manager first.
Agents must leave system installation to you. Verified here with Python 3.14.7.

## Working With Agents

[AGENTS.md](AGENTS.md) is the sole project instruction source. Use an agent with
root `AGENTS.md` support, including current VS Code Copilot, or explicitly ask the
tool to read it before working. Automatic loading depends on the tool and settings.
In VS Code, keep `chat.useAgentsMdFile` enabled and check a new chat's instruction
references or customization diagnostics for this file. Merely creating it does
not prove a particular chat loaded it; no editor settings are changed by this repo.

Describe the next capability and constraints. The agent updates requirements,
implementation, tests, and relevant docs together, then reports actual evidence.
Git commits and pushes remain explicit user actions.

- [Development workflow](docs/development.md): schema, tests, completion, licensing.
- [Requirement register](docs/requirements.toml): authoritative acceptance criteria.
- [Tooling tests](tests/test_workflow.py): executable examples of traceability.

Future kernel work will need GNU Make, a C23-capable `aarch64-none-elf` GCC/binutils
toolchain, `qemu-system-aarch64`, and optionally an AArch64-capable GDB. These are
not required or installed for this foundation; exact versions and commands will
be verified with the first boot requirement.

## License

Original project content: GPL-3.0-or-later, by miaow <guoyr_2013@hotmail.com>.
See [the full GPLv3 text](LICENSES/GPL-3.0-or-later.txt) and per-file SPDX notices.
Third-party content retains its own license; compatibility must be checked before
incorporation or redistribution. No third-party implementation is currently vendored.