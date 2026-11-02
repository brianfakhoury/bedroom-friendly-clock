"""RV32 firmware execution with a deliberately partial C6 MMIO and board model."""

from collections import deque
from datetime import datetime, timezone, timedelta
import struct
import time
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32, UC_HOOK_CODE
from unicorn.riscv_const import UC_RISCV_REG_PC, UC_RISCV_REG_A0
from elftools.elf.elffile import ELFFile
from inspection import hold_location


class Board:
    def __init__(self, elf, realtime=False, monotonic=time.monotonic):
        self.elf = elf
        self.supply_on = True
        self.rtc_stopped = False
        self.powered = True
        self.bootloader = False
        self.boot_down = False
        self.pwr_down = False
        self.pwr_pressed_at = None
        self.cpu = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
        self.cpu.mem_map(0x40800000, 0x80000)
        self.cpu.mem_map(0x50000000, 0x4000)
        self.cpu.mem_map(0x40000000, 0x1000)
        self.cpu.mem_write(0x400000A4, bytes.fromhex("6780000067800000"))
        self.cpu.mem_map(0x20000000, 0x10000)
        self.regs = {}
        self.now = 0
        self.realtime = realtime
        self.monotonic = monotonic
        self.started = monotonic()
        self.out = self.enable = self.status = 0
        self.sleeping = self.waiting = False
        self.wake = 0
        self.console = ""
        self.rx = deque()
        self.touch = False
        self.key_down = False
        self.rotation = 0
        self.epoch = datetime.now(timezone.utc)
        self.epoch_ticks = 0
        self.devices = {
            a: bytearray(65536 if a == 0x5A else 256)
            for a in (0x34, 0x18, 0x40, 0x51, 0x5A, 0x6B)
        }
        self.devices[0x6B][0] = 5
        self.devices[0x6B][0x2E] = 1
        self.pointers = {a: 0 for a in self.devices}
        self.disconnected = set()
        self.scl = self.sda = 1
        self.slave_low = False
        self.phase = "idle"
        self.bit = self.byte = 0
        self.addr = None
        self.ptr_bytes = []
        self.address_byte = True
        self.reading = False
        self.ack = False
        self.panel = bytearray(480 * 480 * 3)
        self.on = False
        self.brightness = 0
        self.cs = 1
        self.spi_bits = self.spi_value = 0
        self.spi = []
        self.pixel_bits = self.pixel = 0
        self.x0 = self.y0 = self.x = self.y = 0
        self.x1 = self.y1 = 479
        self.transactions = 0
        self.pixels = 0
        self.cpu.mmio_map(
            0x60000000, 0x100000, self.read_mmio, None, self.write_mmio, None
        )
        with open(elf, "rb") as stream:
            image = ELFFile(stream)
            self.entry = image.header.e_entry
            self.hold_address = hold_location(image)
            for segment in image.iter_segments():
                if segment.header.p_type == "PT_LOAD":
                    address = segment.header.p_vaddr
                    data = segment.data()
                    self.cpu.mem_write(address, data)
                    # Observe the architectural WFI instruction, not a firmware function.
                    for i in range(0, len(data) - 3, 2):
                        if data[i : i + 4] == b"\x73\x00\x50\x10":
                            self.cpu.hook_add(
                                UC_HOOK_CODE,
                                self.wfi,
                                begin=address + i,
                                end=address + i,
                            )
        self.cpu.hook_add(UC_HOOK_CODE, self.rom, begin=0x400000A4, end=0x400000A8)
        self.cpu.reg_write(UC_RISCV_REG_PC, self.entry)

    @property
    def held(self):
        if not self.powered or self.bootloader:
            return False
        return (
            bool(self.cpu.mem_read(self.hold_address, 1)[0])
            if self.hold_address is not None
            else None
        )

    def rom(self, cpu, address, size, user):
        # Only the two ROM wake-registration ABI calls are modeled. No app hooks.
        if address == 0x400000A4:
            self.wake = cpu.reg_read(UC_RISCV_REG_A0)
        else:
            cpu.reg_write(UC_RISCV_REG_A0, self.wake)

    def wfi(self, cpu, address, size, user):
        cpu.reg_write(UC_RISCV_REG_PC, address + 4)
        self.waiting = True
        cpu.emu_stop()

    def update_time(self):
        if self.realtime:
            self.now = int((self.monotonic() - self.started) * 16000000)

    def run(self, instructions=200000):
        self.update_time()
        if (
            self.powered
            and self.pwr_down
            and self.monotonic() - self.pwr_pressed_at >= 4
        ):
            self.powered = False
            self.on = False
            self.brightness = 0
        if not self.powered or self.bootloader:
            return
        if self.sleeping:
            return
        if self.waiting:
            target = (self.regs.get(0xA01C, 0) << 32) | self.regs.get(0xA020, 0)
            if not (self.status or self.rx):
                if not self.regs.get(0xA000, 0) & (1 << 24):
                    return
                if self.realtime and self.now < target:
                    return
                self.now = max(self.now, target)
            self.waiting = False
        self.cpu.emu_start(
            self.cpu.reg_read(UC_RISCV_REG_PC), 0xFFFFFFFF, count=instructions
        )

    def cold_boot(self, rtc_lost=False):
        self.update_time()
        epoch = self.epoch + timedelta(seconds=(self.now - self.epoch_ticks) / 16000000)
        key, boot = self.key_down, self.boot_down
        pwr, pressed_at = self.pwr_down, self.pwr_pressed_at
        faults, rotation = self.disconnected.copy(), self.rotation
        rtc_stopped = self.rtc_stopped or rtc_lost
        self.__init__(self.elf, self.realtime, self.monotonic)
        self.epoch = epoch
        self.rtc_stopped = rtc_stopped
        self.key_down, self.boot_down = key, boot
        self.pwr_down, self.pwr_pressed_at = pwr, pressed_at
        self.disconnected, self.rotation = faults, rotation
        self.bootloader = boot  # ROM download path is not implemented.

    def input(self, kind, value=None):
        if kind == "supply":
            if value and not self.supply_on:
                self.cold_boot(rtc_lost=True)
            elif not value and self.supply_on:
                self.supply_on = False
                self.powered = False
                self.on = False
                self.brightness = 0
                self.rtc_stopped = True  # No battery is modeled for 5V removal.
        elif kind == "pwr":
            down = bool(value)
            if down and not self.pwr_down:
                if not self.powered and self.supply_on:
                    self.cold_boot()
                self.pwr_pressed_at = self.monotonic()
            self.pwr_down = down
        elif kind == "touch":
            if self.touch == bool(value):
                return
            self.touch = bool(value)
            if self.powered and not self.bootloader:
                self.status |= 1 << 5
            if self.powered and self.sleeping and self.touch:
                if not self.wake:
                    raise RuntimeError("Sleep without registered wake entry")
                self.sleeping = False
                self.waiting = False
                self.cpu.reg_write(UC_RISCV_REG_PC, self.wake)
        elif kind in ("key", "boot"):
            attr = "key_down" if kind == "key" else "boot_down"
            pin = 10 if kind == "key" else 9
            down = True if value is None else bool(value)
            if (
                down
                and not getattr(self, attr)
                and self.powered
                and not self.sleeping
                and not self.bootloader
            ):
                self.status |= 1 << pin
            setattr(self, attr, down if value is not None else False)
        elif kind == "fault":
            addr = int(value["address"])
            if value["enabled"]:
                self.disconnected.add(addr)
            else:
                self.disconnected.discard(addr)
        elif kind == "rotate":
            self.rotation = int(value) % 4
        elif (
            kind == "serial"
            and self.powered
            and not self.bootloader
            and not self.sleeping
        ):
            self.rx.extend((str(value) + "\n").encode("ascii"))

    def read_mmio(self, cpu, offset, size, user):
        if offset == 0x9103C:
            pins = (self.out & self.enable) | (~self.enable & 0xFFFFFFFF)
            if self.slave_low:
                pins &= ~(1 << 8)
            if self.touch:
                pins &= ~(1 << 5)
            if self.key_down:
                pins &= ~(1 << 10)
            if self.boot_down:
                pins &= ~(1 << 9)
            return pins
        if offset in (0x91044, 0xB2018):
            return self.status
        if offset == 0xA004:
            return 1 << 29
        if offset == 0xA040:
            return self.now >> 32
        if offset == 0xA044:
            return self.now & 0xFFFFFFFF
        if offset == 0xA068:
            target = (self.regs.get(0xA01C, 0) << 32) | self.regs.get(0xA020, 0)
            return int(
                bool(self.regs.get(0xA000, 0) & (1 << 24)) and self.now >= target
            )
        if offset == 0xF004:
            return 2 | (4 if self.rx else 0)
        if offset == 0xF000:
            return self.rx.popleft() if self.rx else 0
        if offset == 0xF008:
            return 4 if self.rx else 0
        if offset in (0xAF800, 0xAF804):
            return 0  # analog I2C busy clears
        return self.regs.get(offset, 0)

    def write_mmio(self, cpu, offset, size, value, user):
        self.regs[offset] = value
        if offset == 0xA004:
            if self.realtime:
                self.update_time()
            else:
                self.now += 1600  # accelerated mode for deterministic scenario tests
        elif offset == 0xF000:
            self.console = (self.console + chr(value & 255))[-8192:]
        elif offset in (0x9104C, 0xB2020):
            self.status &= ~value
        elif offset == 0xB0120 and value & (1 << 31):
            self.sleeping = True
            self.cpu.emu_stop()
        elif offset in (0x91008, 0x9100C, 0x91024, 0x91028):
            old = self.out
            if offset == 0x91008:
                self.out |= value
            if offset == 0x9100C:
                self.out &= ~value
            if offset == 0x91024:
                self.enable |= value
            if offset == 0x91028:
                self.enable &= ~value
            pins = self.out | ~self.enable
            self.i2c_edges((pins >> 7) & 1, (pins >> 8) & 1)
            self.panel_edges(old, self.out)

    def refresh_device(self, addr):
        self.update_time()
        d = self.devices[addr]
        if addr == 0x51 and not d[0] & 0x20:
            t = self.epoch + timedelta(seconds=(self.now - self.epoch_ticks) / 16000000)
            values = [
                t.second,
                t.minute,
                t.hour,
                t.day,
                (t.weekday() + 1) % 7,
                t.month,
                t.year - 2000,
            ]
            d[4:11] = bytes((v // 10) * 16 + v % 10 for v in values)
            if self.rtc_stopped:
                d[4] |= 0x80
        elif addr == 0x5A:
            d[0xD000:0xD00A] = bytes([6, 0, 0, 0, 0, int(self.touch), 0xAB, 0, 0, 0])
        elif addr == 0x6B:
            xyz = [(0, 16384, 0), (16384, 0, 0), (0, -16384, 0), (-16384, 0, 0)][
                self.rotation
            ]
            d[0x35:0x3B] = struct.pack("<hhh", *xyz)

    def received(self, value):
        if self.address_byte:
            self.addr = value >> 1
            self.reading = bool(value & 1)
            self.address_byte = False
            self.ptr_bytes = []
            self.ack = self.addr in self.devices and self.addr not in self.disconnected
            if self.ack:
                self.refresh_device(self.addr)
                self.transactions += 1
        elif self.addr in self.devices:
            width = 2 if self.addr == 0x5A else 1
            if len(self.ptr_bytes) < width:
                self.ptr_bytes.append(value)
                self.pointers[self.addr] = int.from_bytes(bytes(self.ptr_bytes), "big")
            else:
                pointer = self.pointers[self.addr]
                self.devices[self.addr][pointer] = value
                if self.addr == 0x51 and pointer == 4:
                    self.rtc_stopped = bool(value & 0x80)
                self.pointers[self.addr] = (pointer + 1) % len(self.devices[self.addr])
                if self.addr == 0x51 and pointer == 0 and not value & 0x20:
                    b = self.devices[0x51]
                    dec = lambda v: (v >> 4) * 10 + (v & 15)
                    try:
                        self.epoch = datetime(
                            2000 + dec(b[10]),
                            dec(b[9]),
                            dec(b[7]),
                            dec(b[6]),
                            dec(b[5]),
                            dec(b[4]),
                            tzinfo=timezone.utc,
                        )
                        self.epoch_ticks = self.now
                    except ValueError:
                        pass
            self.ack = True
        else:
            self.ack = False

    def send_bit(self):
        pointer = self.pointers[self.addr]
        value = self.devices[self.addr][pointer]
        self.slave_low = not bool(value & (1 << (7 - self.bit)))

    def i2c_edges(self, scl, sda):
        # START/STOP are master SDA transitions while SCL is high.
        if scl and self.scl and sda != self.sda:
            if not sda:
                self.phase = "receive"
                self.bit = self.byte = 0
                self.address_byte = True
                self.slave_low = False
            else:
                self.phase = "idle"
                self.slave_low = False
        elif not self.scl and scl:
            if self.phase == "receive":
                self.byte = (self.byte << 1) | sda
                self.bit += 1
                if self.bit == 8:
                    self.received(self.byte)
                    self.phase = "ack_start"
            elif self.phase == "ack":
                self.phase = "ack_end"
            elif self.phase == "send":
                self.bit += 1
                if self.bit == 8:
                    self.phase = "send_end"
            elif self.phase == "master_ack":
                self.ack = not sda
                self.phase = "next_byte"
        elif self.scl and not scl:
            if self.phase == "ack_start":
                self.slave_low = self.ack
                self.phase = "ack"
            elif self.phase == "ack_end":
                self.slave_low = False
                self.bit = self.byte = 0
                self.phase = "send" if self.reading and self.ack else "receive"
                if self.phase == "send":
                    self.send_bit()
            elif self.phase == "send":
                self.send_bit()
            elif self.phase == "send_end":
                self.slave_low = False
                self.phase = "master_ack"
                self.pointers[self.addr] = (self.pointers[self.addr] + 1) % len(
                    self.devices[self.addr]
                )
            elif self.phase == "next_byte":
                self.bit = 0
                self.phase = "send" if self.ack else "idle"
                if self.ack:
                    self.send_bit()
        self.scl, self.sda = scl, sda

    def panel_edges(self, old, new):
        cs = (new >> 15) & 1
        if self.cs and not cs:
            self.spi = []
            self.spi_bits = self.spi_value = 0
            self.pixel_bits = self.pixel = 0
        if not self.cs and cs:
            if len(self.spi) >= 4:
                cmd, data = self.spi[2], self.spi[4:]
                if cmd == 0x29:
                    self.on = True
                if cmd == 0x28:
                    self.on = False
                if cmd == 0x51 and data:
                    self.brightness = data[0]
                if cmd in (0x2A, 0x2B) and len(data) == 4:
                    lo, hi = data[0] * 256 + data[1], data[2] * 256 + data[3]
                    if not 0 <= lo <= hi < 480:
                        raise RuntimeError("Invalid panel window")
                    if cmd == 0x2A:
                        self.x0, self.x1 = lo, hi
                    else:
                        self.y0, self.y1 = lo, hi
                if cmd == 0x2C:
                    self.x, self.y = self.x0, self.y0
        if not cs and not (old & 1) and new & 1:
            if len(self.spi) >= 4 and self.spi[0] == 0x32:
                self.pixel = (self.pixel << 4) | ((new >> 1) & 15)
                self.pixel_bits += 4
                if self.pixel_bits == 16:
                    color = self.pixel
                    index = (self.y * 480 + self.x) * 3
                    self.panel[index : index + 3] = bytes(
                        (
                            ((color >> 11) & 31) * 255 // 31,
                            ((color >> 5) & 63) * 255 // 63,
                            (color & 31) * 255 // 31,
                        )
                    )
                    self.pixels += 1
                    self.x += 1
                    if self.x > self.x1:
                        self.x = self.x0
                        self.y += 1
                    if self.y > self.y1:
                        self.y = self.y0
                    self.pixel_bits = self.pixel = 0
            else:
                self.spi_value = (self.spi_value << 1) | ((new >> 1) & 1)
                self.spi_bits += 1
                if self.spi_bits == 8:
                    self.spi.append(self.spi_value)
                    self.spi_value = self.spi_bits = 0
        self.cs = cs
