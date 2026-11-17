# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import gdb

PAGE_OFFSET = 0xffffffc000000000
PHYS_OFFSET = 0x40000000


def value(expression):
    # GDB types x0-x30 as signed long; high kernel addresses must compare unsigned.
    result = gdb.parse_and_eval(expression)
    if expression == "$SCTLR" and result.type.code == gdb.TYPE_CODE_VOID:
        result = gdb.parse_and_eval("$SCTLR_EL1")
    return int(result) & 0xffffffffffffffff


def physical(symbol):
    return value(f"&{symbol}") - PAGE_OFFSET + PHYS_OFFSET


def stop_at(symbol, before_mmu=False):
    """Code before the MMU switch runs at the physical alias of its link address."""
    address = physical(symbol) if before_mmu else value(f"&{symbol}")
    breakpoint = gdb.Breakpoint(f"*{address}", type=gdb.BP_HARDWARE_BREAKPOINT)
    gdb.execute("continue")
    assert value("$pc") == address, f"Did not reach {symbol}"
    breakpoint.delete()


def set_control(control):
    """Execute MSR SCTLR_EL1, x0; ISB; BR x1 outside the kernel image."""
    inferior = gdb.selected_inferior()
    address = 0x40070000
    instructions = (0xd5181000, 0xd5033fdf, 0xd61f0020)
    code = b"".join(word.to_bytes(4, "little") for word in instructions)
    saved = bytes(inferior.read_memory(address, len(code)))
    registers = {name: value(f"${name}") for name in ("pc", "x0", "x1")}
    inferior.write_memory(address, code)
    gdb.execute(f"set $x0 = {control}")
    gdb.execute(f"set $x1 = {registers['pc']}")
    gdb.execute(f"set $pc = {address}")
    breakpoint = gdb.Breakpoint(f"*{registers['pc']}", type=gdb.BP_HARDWARE_BREAKPOINT)
    gdb.execute("continue")
    assert value("$pc") == registers["pc"]
    breakpoint.delete()
    inferior.write_memory(address, saved)
    for name, original in registers.items():
        gdb.execute(f"set ${name} = {original}")


def readable(address):
    try:
        gdb.selected_inferior().read_memory(address, 4)
    except gdb.MemoryError:
        return False
    return True


def instructions(symbol, before_mmu=False):
    virtual = value(f"&{symbol}")
    block = gdb.block_for_pc(virtual)
    count = (block.end - virtual) // 4 if block else 32
    address = physical(symbol) if before_mmu else virtual
    code = gdb.selected_inferior().architecture().disassemble(address, count=count)
    for instruction in code:
        instruction["asm"] = " ".join(instruction["asm"].split())
    return code


class MaintenanceTrace(gdb.Breakpoint):
    def __init__(self, address, instruction, events):
        super().__init__(f"*{address}", type=gdb.BP_HARDWARE_BREAKPOINT, internal=True)
        self.instruction = instruction
        self.events = events

    def stop(self):
        if self.instruction == "ic iallu":
            assert value("$SCTLR") & 0x1005 == 0
        self.events.append(self.instruction)
        return False


def check_tlbi(events, expected):
    indexes = [index for index, event in enumerate(events) if event.startswith("tlbi ")]
    assert len(indexes) == expected, events
    for index in indexes:
        assert events[index - 1:index + 3] == ["dsb ishst", "tlbi vmalle1", "dsb ish", "isb"], events


def boot():
    stop_at("_start", before_mmu=True)
    start = physical("__bss_start")
    end = physical("__bss_stop")
    inferior = gdb.selected_inferior()
    inferior.write_memory(start, b"\xff" * (end - start))
    stop_at("mmu_init", before_mmu=True)
    expected = bytearray(end - start)
    offset = physical("__fdt_pointer") - start
    expected[offset:offset + 8] = value("$x19").to_bytes(8, "little")
    assert bytes(inferior.read_memory(start, end - start)) == expected
    assert bytes(inferior.read_memory(physical("bss_probe"), 8)) == bytes(8)
    control = value("$SCTLR")
    set_control(control | 2)
    assert value("$SCTLR") & 2
    stop_at("__mmap_switched")
    assert value("$SCTLR") & 3 == 1
    image = physical("_text")
    assert bytes(inferior.read_memory(image, 64)) == \
        bytes(inferior.read_memory(value("&_text"), 64)), "Identity map missing"
    assert readable(PHYS_OFFSET + 0x07fff000), "Identity map does not cover all RAM"
    stop_at("start_kernel")
    after = value("$SCTLR")
    assert after & 3 == 1
    assert after & 0x18 == control & 0x18
    assert value("$sp") == value("&__stack_top")
    assert not readable(image), "Identity map still present"
    assert not readable(0x09000000)
    assert readable(value("&_text"))
    stop_at("start_apps")
    print("GDB BOOT OK")


def cache():
    stop_at("mmu_init", before_mmu=True)
    events = []
    probes = []
    for symbol, low in (("mmu_init", True), ("flush_tlb_all", True),
                        ("cpu_uninstall_idmap", False), ("flush_tlb_all", False)):
        for instruction in instructions(symbol, before_mmu=True):
            operation = instruction["asm"]
            if operation.split()[0] in ("dsb", "isb", "tlbi", "ic"):
                address = instruction["addr"] + (0 if low else PAGE_OFFSET - PHYS_OFFSET)
                probes.append(MaintenanceTrace(address, operation, events))
    stop_at("__mmap_switched")
    assert value("$SCTLR") & 0x1004 == 0x1004
    check_tlbi(events, 1)
    index = events.index("ic iallu")
    assert events[index:index + 3] == ["ic iallu", "dsb sy", "isb"], events
    stop_at("start_kernel")
    assert value("$SCTLR") & 0x1004 == 0x1004
    check_tlbi(events, 2)
    for probe in probes:
        probe.delete()
    print("GDB CACHE BOOT OK")
    for identifier in range(3):
        stop_at("flush_icache_range")
        start, end = value("$x0"), value("$x1")
        assert start == PAGE_OFFSET + 0x01000000 + identifier * 0x20000
        assert end - start == value(f"tasks[{identifier}].size")
        assert value("$SCTLR") & 0x1004 == 0x1004
        line = 4 << ((value("$CTR_EL0") >> 16) & 15)
        code = instructions("flush_icache_range")
        clean = next(instruction["addr"] for instruction in code if instruction["asm"] == "dc cvau, x4")
        barrier = next(index for index, instruction in enumerate(code) if instruction["asm"] == "dsb sy")
        expected = ["dsb sy", "ic iallu", "dsb sy", "isb", "ret"]
        assert [instruction["asm"] for instruction in code[barrier:barrier + 5]] == expected
        probe = gdb.Breakpoint(f"*{clean}", type=gdb.BP_HARDWARE_BREAKPOINT, internal=True)
        for address in range(start & -line, end, line):
            gdb.execute("continue")
            assert value("$pc") == clean
            assert value("$x4") == address
            assert value("$x2") == line
        probe.delete()
        stop = code[barrier]["addr"]
        probe = gdb.Breakpoint(f"*{stop}", type=gdb.BP_HARDWARE_BREAKPOINT, internal=True)
        gdb.execute("continue")
        assert value("$pc") == stop
        probe.delete()
        for instruction in code[barrier:barrier + 5]:
            assert value("$pc") == instruction["addr"]
            gdb.execute("stepi")
        print(f"GDB CACHE app={identifier} lines={len(range(start & -line, end, line))} OK")
    stop_at("enter_app")
    print("GDB CACHE OK")


def preempt():
    counts = [0, 0, 0]
    for tick in range(9):
        stop_at("trap")
        assert value("$x1") == 1
        assert value("$cpsr") & 0x8f == 0x85
        identifier = value("current")
        assert identifier == tick % 3
        frame = gdb.parse_and_eval("*(struct pt_regs *)$x0")
        task = gdb.parse_and_eval(f"tasks[{identifier}]")
        assert int(task["image"]) <= int(frame["pc"]) < int(task["image"] + task["size"])
        assert int(frame["pstate"]) & 0xf000008f == 0x60000000
        assert int(frame["sp"]) == int(task["stack_top"]) - 16
        for register in range(1, 31):
            assert int(frame["regs"][register]) == 100 + register, register
        progress = int(frame["regs"][0])
        assert progress > counts[identifier]
        counts[identifier] = progress
        stop_at("enter_app")
        selected = (identifier + 1) % 3
        assert value("current") == selected
        assert value("$x0") == value(f"&tasks[{selected}].regs")
        print(f"GDB TICK app={identifier} progress={progress:016x} context=OK")
    print("GDB PREEMPT OK")


def survivor():
    repeats = 0
    for _ in range(30):
        stop_at("trap")
        before = value("current")
        stop_at("enter_app")
        after = value("current")
        assert value(f"tasks[{after}].runnable")
        assert value("$x0") == value(f"&tasks[{after}].regs")
        print(f"GDB SWITCH {before} -> {after}")
        if not value("tasks[0].runnable") and not value("tasks[1].runnable"):
            assert after == 2
            if before == 2:
                repeats += 1
        if repeats == 3:
            print("GDB SURVIVOR OK")
            return
    raise AssertionError("Survivor did not receive three timer rounds")