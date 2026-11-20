# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import gdb

PAGE_OFFSET = 0xffffffc000000000
PHYS_OFFSET = 0x40000000
PAGE_SIZE = 4096
APP_LOAD_ADDR = 0x400000
APP_STACK_SIZE = 0x4000
STACK_TOP = 0x8000000000
ADDRESS = 0x0000fffffffff000
NORMAL = 0x707
USER = 1 << 6
RDONLY = 1 << 7
PXN = 1 << 53
UXN = 1 << 54


def value(expression):
    # GDB types x0-x30 as signed long; high kernel addresses must compare unsigned.
    result = gdb.parse_and_eval(expression)
    if expression == "$SCTLR" and result.type.code == gdb.TYPE_CODE_VOID:
        result = gdb.parse_and_eval("$SCTLR_EL1")
    return int(result) & 0xffffffffffffffff


def physical(symbol):
    return value(f"&{symbol}") - PAGE_OFFSET + PHYS_OFFSET


def linear(address):
    return address - PHYS_OFFSET + PAGE_OFFSET


def word(address):
    return int.from_bytes(gdb.selected_inferior().read_memory(address, 8), "little")


def task_root(identifier):
    return value(f"tasks[{identifier}].mm->pgd") - PAGE_OFFSET + PHYS_OFFSET


def task_pte(identifier, address):
    table = linear(task_root(identifier))
    for shift in (30, 21):
        entry = word(table + ((address >> shift) & 511) * 8)
        if not entry:
            return 0
        assert entry & 3 == 3, hex(entry)
        table = linear(entry & ADDRESS)
    return word(table + ((address >> 12) & 511) * 8)


def user_mappings(identifier):
    inferior = gdb.selected_inferior()
    tables = []
    mappings = {}

    def read_table(address):
        contents = bytes(inferior.read_memory(linear(address), PAGE_SIZE))
        tables.append(address)
        return [int.from_bytes(contents[i:i + 8], "little") for i in range(0, PAGE_SIZE, 8)]

    for upper, pgd in enumerate(read_table(task_root(identifier))):
        if pgd:
            assert pgd & ~ADDRESS == 3
            for middle, pmd in enumerate(read_table(pgd & ADDRESS)):
                if pmd:
                    assert pmd & ~ADDRESS == 3
                    for lower, pte in enumerate(read_table(pmd & ADDRESS)):
                        if pte:
                            mappings[(upper << 30) | (middle << 21) | (lower << 12)] = pte
    return tables, mappings


def kernel_tables(root):
    inferior = gdb.selected_inferior()
    result = []

    def read_table(address):
        contents = bytes(inferior.read_memory(linear(address), PAGE_SIZE))
        result.append((address, contents))
        return [int.from_bytes(contents[i:i + 8], "little") for i in range(0, PAGE_SIZE, 8)]

    for pgd in read_table(root):
        if pgd:
            assert pgd & 3 == 3
            for pmd in read_table(pgd & ADDRESS):
                if pmd:
                    assert pmd & 3 == 3
                    read_table(pmd & ADDRESS)
    return result


def set_task_register(identifier, register, contents):
    gdb.execute(f"set var tasks[{identifier}].regs.regs[{register}] = {contents}")


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
        offset = 0
        while True:
            stop_at("flush_icache_range")
            start, end = value("$x0"), value("$x1")
            size = value(f"tasks[{identifier}].size")
            descriptor = task_pte(identifier, APP_LOAD_ADDR + offset)
            assert start == linear(descriptor & ADDRESS)
            assert end - start == min(PAGE_SIZE, size - offset)
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
            print(f"GDB CACHE app={identifier} page={offset // PAGE_SIZE} "
                  f"lines={len(range(start & -line, end, line))} OK")
            offset += PAGE_SIZE
            if offset >= size:
                break
    stop_at("enter_app")
    print("GDB CACHE OK")


def address_spaces():
    stop_at("enter_app")
    inferior = gdb.selected_inferior()
    roots = [task_root(identifier) for identifier in range(3)]
    assert len(set(roots)) == 3
    assert value("$TTBR0_EL1") == roots[0]
    kernel_root = value("$TTBR1_EL1")
    kernel_before = kernel_tables(kernel_root)
    backing = []
    tables = []

    for identifier in range(3):
        assert value(f"tasks[{identifier}].image") == APP_LOAD_ADDR
        assert value(f"tasks[{identifier}].stack_top") == STACK_TOP
        size = value(f"tasks[{identifier}].size")
        source = bytes(inferior.read_memory(value(f"&app{identifier}_start"), size))
        copied = bytearray()
        task_tables, mappings = user_mappings(identifier)
        tables.extend(task_tables)
        image_addresses = list(range(APP_LOAD_ADDR, APP_LOAD_ADDR + size, PAGE_SIZE))
        stack_addresses = list(range(STACK_TOP - APP_STACK_SIZE, STACK_TOP, PAGE_SIZE))
        assert set(mappings) == set(image_addresses + stack_addresses)
        for offset in range(0, size, PAGE_SIZE):
            descriptor = mappings[APP_LOAD_ADDR + offset]
            assert descriptor & ~ADDRESS == NORMAL | USER | RDONLY | PXN
            backing.append(descriptor & ADDRESS)
            copied.extend(inferior.read_memory(linear(descriptor & ADDRESS), PAGE_SIZE))
        assert bytes(copied[:size]) == source
        assert bytes(copied[size:]) == bytes(len(copied) - size)
        for address in stack_addresses:
            descriptor = mappings[address]
            assert descriptor & ~ADDRESS == NORMAL | USER | PXN | UXN
            backing.append(descriptor & ADDRESS)
            assert bytes(inferior.read_memory(linear(descriptor & ADDRESS), PAGE_SIZE)) == bytes(PAGE_SIZE)
        for address in (0, APP_LOAD_ADDR - PAGE_SIZE, APP_LOAD_ADDR + 0x10000,
                        STACK_TOP - APP_STACK_SIZE - PAGE_SIZE,
                        0x41000000, 0x41020000, 0x41040000):
            assert task_pte(identifier, address) == 0, hex(address)
    allocated = backing + tables
    assert len(allocated) == len(set(allocated))
    reserved = []
    for index in range(value("memblock.reserved.cnt")):
        start = value(f"memblock.reserved.regions[{index}].base")
        size = value(f"memblock.reserved.regions[{index}].size")
        reserved.append((start, start + size))
    for address in allocated:
        assert PHYS_OFFSET <= address < PHYS_OFFSET + 0x08000000
        assert all(address + PAGE_SIZE <= start or address >= end for start, end in reserved)
    for identifier in range(3):
        for other in range(3):
            if identifier != other:
                for address in (APP_LOAD_ADDR, STACK_TOP - PAGE_SIZE):
                    assert task_pte(identifier, task_pte(other, address) & ADDRESS) == 0

    events = []
    probes = []
    for instruction in instructions("flush_tlb_all"):
        operation = instruction["asm"]
        if operation.split()[0] in ("dsb", "isb", "tlbi"):
            probes.append(MaintenanceTrace(instruction["addr"], operation, events))
    for tick in range(9):
        stop_at("trap")
        assert value("$x1") == 1
        assert value("current") == tick % 3
        stop_at("enter_app")
        selected = (tick + 1) % 3
        assert value("current") == selected
        assert value("$TTBR0_EL1") == roots[selected]
        assert value("$TTBR1_EL1") == kernel_root
        check_tlbi(events, tick + 1)
        print(f"GDB VM switch={tick + 1} app={selected} ttbr0={roots[selected]:016x} OK")
    for probe in probes:
        probe.delete()
    assert kernel_tables(kernel_root) == kernel_before
    print("GDB VM OK")


def same_va():
    stop_at("enter_app")
    inferior = gdb.selected_inferior()
    address = STACK_TOP - PAGE_SIZE
    for identifier in range(3):
        descriptor = task_pte(identifier, address)
        inferior.write_memory(linear(descriptor & ADDRESS), bytes([ord("A") + identifier]))
        set_task_register(identifier, 0, address)
        set_task_register(identifier, 1, 1)
    stop_at("timer_stop")
    print("GDB SAME VA OK")


def cross_task():
    stop_at("enter_app")
    addresses = [
        task_pte(1, APP_LOAD_ADDR) & ADDRESS,
        task_pte(2, STACK_TOP - PAGE_SIZE) & ADDRESS,
        0x41020000,
    ]
    for identifier, address in enumerate(addresses):
        assert not task_pte(identifier, address)
        set_task_register(identifier, 0, address)
        set_task_register(identifier, 1, -14)
        print(f"GDB PROBE app={identifier} address={address:016x}")
    stop_at("timer_stop")
    print("GDB CROSS TASK OK")


def allocation_failure(number):
    stop_at("start_apps")
    for _ in range(number):
        stop_at("alloc_pages")
    gdb.execute("return (struct page *)0")
    stop_at("kernel_panic")
    print(f"GDB ALLOCATION FAILURE {number} OK")


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