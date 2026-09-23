# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import gdb


def value(expression):
    return int(gdb.parse_and_eval(expression))


def stop_at(symbol):
    breakpoint = gdb.Breakpoint(f"*{symbol}", type=gdb.BP_HARDWARE_BREAKPOINT)
    gdb.execute("continue")
    assert value("$pc") == value(f"&{symbol}"), f"Did not reach {symbol}"
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


def boot():
    stop_at("_start")
    start = value("&__bss_start")
    end = value("&__bss_end")
    inferior = gdb.selected_inferior()
    inferior.write_memory(start, b"\xff" * (end - start))
    stop_at("mmu_init")
    expected = bytearray(end - start)
    offset = value("&dtb_pointer") - start
    expected[offset:offset + 8] = value("dtb_pointer").to_bytes(8, "little")
    assert bytes(inferior.read_memory(start, end - start)) == expected
    assert bytes(inferior.read_memory(value("&bss_probe"), 8)) == bytes(8)
    control = value("$SCTLR")
    set_control(control | 2)
    assert value("$SCTLR") & 2
    stop_at("kernel_main")
    after = value("$SCTLR")
    assert after & 0x1007 == 1
    assert after & 0x18 == control & 0x18
    stop_at("start_apps")
    print("GDB BOOT OK")


def preempt():
    counts = [0, 0, 0]
    for tick in range(9):
        stop_at("trap")
        assert value("$x1") == 1
        assert value("$cpsr") & 0x8f == 0x85
        identifier = value("current")
        assert identifier == tick % 3
        frame = gdb.parse_and_eval("*(struct context *)$x0")
        task = gdb.parse_and_eval(f"tasks[{identifier}]")
        assert int(task["image"]) <= int(frame["pc"]) < int(task["image"] + task["size"])
        assert int(frame["pstate"]) & 0xf000008f == 0x60000000
        assert int(frame["sp"]) == int(task["stack_top"]) - 16
        for register in range(1, 31):
            assert int(frame["registers"][register]) == 100 + register, register
        progress = int(frame["registers"][0])
        assert progress > counts[identifier]
        counts[identifier] = progress
        stop_at("enter_app")
        selected = (identifier + 1) % 3
        assert value("current") == selected
        assert value("$x0") == value(f"&tasks[{selected}].context")
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
        assert value("$x0") == value(f"&tasks[{after}].context")
        print(f"GDB SWITCH {before} -> {after}")
        if not value("tasks[0].runnable") and not value("tasks[1].runnable"):
            assert after == 2
            if before == 2:
                repeats += 1
        if repeats == 3:
            print("GDB SURVIVOR OK")
            return
    raise AssertionError("Survivor did not receive three timer rounds")