"""Read-only debugger observations; programmed settings are not power measurements."""

from unicorn.riscv_const import UC_RISCV_REG_PC


def hold_location(image):
    symbols = image.get_section_by_name(".symtab")
    matches = symbols.get_symbol_by_name("clock") if symbols else None
    if not matches or not image.has_dwarf_info():
        return None
    for cu in image.get_dwarf_info().iter_CUs():
        for die in cu.iter_DIEs():
            name = die.attributes.get("DW_AT_name")
            if die.tag != "DW_TAG_variable" or not name or name.value != b"clock":
                continue
            typ = die.get_DIE_from_attribute("DW_AT_type")
            while typ.tag in (
                "DW_TAG_typedef",
                "DW_TAG_const_type",
                "DW_TAG_volatile_type",
            ):
                typ = typ.get_DIE_from_attribute("DW_AT_type")
            for member in typ.iter_children():
                name = member.attributes.get("DW_AT_name")
                offset = member.attributes.get("DW_AT_data_member_location")
                if (
                    name
                    and name.value == b"held"
                    and offset
                    and isinstance(offset.value, int)
                ):
                    return matches[0].entry.st_value + offset.value
    return None


def snapshot(board):
    regs = board.regs

    def value(offset):
        return f"0x{regs[offset]:08x}" if offset in regs else "not written"

    modes = {}
    for name, offset in [("Active", 0xB0000), ("Sleep", 0xB0068)]:
        raw = regs.get(offset)
        modes[name] = {
            domain: (
                "power-down requested" if raw & (1 << bit) else "no power-down request"
            )
            if raw is not None
            else "not configured"
            for domain, bit in [
                ("HP CPU", 29),
                ("HP AON", 30),
                ("HP TOP", 31),
                ("Wi-Fi", 27),
                ("Flash supply", 21),
            ]
        }
    clocks = {}
    for name, offset, bit in [
        ("SYSTIMER functional clock", 0x96058, 22),
        ("USB peripheral clock", 0x9608C, 0),
        ("Hardware I2C APB", 0x96020, 0),
        ("SPI2 APB", 0x960C0, 0),
        ("I2S APB", 0x9606C, 0),
        ("UART0 APB", 0x96000, 0),
        ("UART1 APB", 0x9600C, 0),
    ]:
        clocks[name] = (
            ("enabled" if regs[offset] & (1 << bit) else "disabled")
            if offset in regs
            else "not written"
        )
    return {
        "Execution (modeled)": {
            "HP CPU": "powered off"
            if not board.powered
            else "ROM download (not modeled)"
            if board.bootloader
            else "deep sleep"
            if board.sleeping
            else "waiting (WFI)"
            if board.waiting
            else "running",
            "PC": f"0x{board.cpu.reg_read(UC_RISCV_REG_PC):08x}",
            "LP CPU": "not executed",
            "LP SRAM": "retained" if board.powered else "discarded on power-on",
            "Elapsed seconds": round(board.now / 16000000, 3),
            "Registered wake entry": f"0x{board.wake:08x}",
            "Display": "on" if board.on else "off",
            "Hold": board.held,
        },
        "Active power policy (programmed)": modes["Active"],
        "Sleep power policy (programmed)": modes["Sleep"],
        "Clock gates (programmed, not enforced)": clocks,
        "GPIO and timer": {
            "GPIO outputs": f"0x{board.out:08x}",
            "GPIO output enables": f"0x{board.enable:08x}",
            "Pending input edges": f"0x{board.status:08x}",
            "Pad holds": value(0xB102C),
            "SYSTIMER config": value(0xA000),
            "Timer target ticks": (regs.get(0xA01C, 0) << 32) | regs.get(0xA020, 0),
            "Wake source enable": value(0xB0128),
        },
        "Power and watchdog registers (programmed)": {
            "Radio force control": value(0xB0104),
            "Modem clocks": value(0xA9804),
            "Modem clocks 1": value(0xA9814),
            "Modem LP clocks": value(0xAF018),
            "Active clock power": value(0xB0014),
            "Sleep clock power": value(0xB007C),
            "LP watchdog config": value(0xB1C00),
            "LP watchdog timeout": value(0xB1C04),
            "Watchdog expiry": "not modeled",
            "Power consumption": "not modeled",
        },
    }
