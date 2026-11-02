"""Timing contracts use a controllable monotonic clock, never sleep-based guesses."""

import unittest
from pathlib import Path
from board import Board
from unicorn.riscv_const import UC_RISCV_REG_PC

ELF = Path(__file__).resolve().parents[1] / "build/clock.elf"


class TimingTests(unittest.TestCase):
    def setUp(self):
        self.wall = 1000.0
        self.b = Board(ELF, realtime=True, monotonic=lambda: self.wall)

    def test_duplicate_touch_release_does_not_create_an_edge(self):
        self.b.input("touch", False)
        self.assertEqual(self.b.status, 0)
        self.b.input("touch", True)
        self.assertEqual(self.b.status, 1 << 5)
        self.b.status = 0
        self.b.input("touch", True)
        self.assertEqual(self.b.status, 0)
        self.b.input("touch", False)
        self.assertEqual(self.b.status, 1 << 5)

    def test_supply_cycle_preserves_held_pwr_button(self):
        self.b.input("pwr", True)
        self.b.input("supply", False)
        self.b.input("supply", True)
        self.assertTrue(self.b.pwr_down)
        self.wall += 4.1
        self.b.run()
        self.assertFalse(self.b.powered)

    def test_key_can_be_held_during_sleep_without_waking(self):
        self.b.sleeping = True
        self.b.input("key", True)
        self.assertTrue(self.b.key_down)
        self.assertTrue(self.b.sleeping)
        self.assertEqual(self.b.status, 0)
        self.assertFalse(self.b.read_mmio(None, 0x9103C, 4, None) & (1 << 10))
        self.b.input("key", False)
        self.assertFalse(self.b.key_down)
        self.assertTrue(self.b.read_mmio(None, 0x9103C, 4, None) & (1 << 10))

    def test_5v_removal_blocks_wake_and_pwr_then_cold_boots(self):
        self.b.sleeping = True
        self.b.input("supply", False)
        self.assertFalse(self.b.powered)
        self.assertFalse(self.b.supply_on)
        self.b.input("pwr", True)
        self.b.input("touch", True)
        self.b.run()
        self.assertFalse(self.b.powered)
        self.b.input("supply", True)
        self.assertTrue(self.b.powered)
        self.assertTrue(self.b.supply_on)
        self.assertFalse(self.b.sleeping)
        self.assertEqual(self.b.cpu.reg_read(UC_RISCV_REG_PC), self.b.entry)
        self.b.refresh_device(0x51)
        self.assertTrue(self.b.devices[0x51][4] & 0x80)
        self.assertFalse(self.b.held)

    def test_boot_pin_and_single_press_edge(self):
        self.b.input("boot", True)
        self.assertEqual(self.b.status, 1 << 9)
        self.assertFalse(self.b.read_mmio(None, 0x9103C, 4, None) & (1 << 9))
        self.b.status = 0
        self.b.input("boot", True)
        self.assertEqual(self.b.status, 0)
        self.b.input("boot", False)
        self.assertTrue(self.b.read_mmio(None, 0x9103C, 4, None) & (1 << 9))

    def test_power_cycle_preserves_physical_key_and_resets_execution(self):
        self.b.sleeping = True
        self.b.input("pwr", True)
        self.wall += 3.9
        self.b.run()
        self.assertTrue(self.b.powered)
        self.wall += 0.2
        self.b.run()
        self.assertFalse(self.b.powered)
        self.b.input("touch", True)
        self.assertFalse(self.b.powered)
        self.b.input("key", True)
        self.b.input("pwr", False)
        self.b.input("pwr", True)
        self.assertTrue(self.b.powered)
        self.assertTrue(self.b.key_down)
        self.assertFalse(self.b.sleeping)
        self.assertEqual(self.b.cpu.reg_read(UC_RISCV_REG_PC), self.b.entry)

    def test_boot_held_at_power_on_does_not_run_application(self):
        self.b.powered = False
        self.b.input("boot", True)
        self.b.input("pwr", True)
        self.assertTrue(self.b.bootloader)
        self.b.run()
        self.assertEqual(self.b.cpu.reg_read(UC_RISCV_REG_PC), self.b.entry)
        self.assertEqual(self.b.console, "")

    def test_timer_reads_do_not_advance_time(self):
        self.wall += 1
        for _ in range(10000):
            self.b.write_mmio(None, 0xA004, 4, 1 << 30, None)
        self.assertEqual(self.b.now, 16000000)

    def test_rtc_tracks_seconds_even_during_sleep(self):
        before = self.b.epoch
        self.b.sleeping = True
        self.wall += 60
        self.b.run()
        self.b.refresh_device(0x51)
        d = self.b.devices[0x51]
        dec = lambda v: (v >> 4) * 10 + (v & 15)
        self.assertEqual(dec(d[4]), before.second)
        self.assertEqual(dec(d[5]), (before.minute + 1) % 60)
        self.assertEqual(self.b.now, 60 * 16000000)

    def test_wait_does_not_jump_to_future_deadline(self):
        self.b.waiting = True
        self.b.regs[0xA000] = 1 << 24
        self.b.regs[0xA020] = 3 * 16000000
        for _ in range(100):
            self.b.run()
        self.assertTrue(self.b.waiting)
        self.assertEqual(self.b.now, 0)
        self.wall += 2.99
        self.b.run()
        self.assertTrue(self.b.waiting)
        self.assertLess(self.b.now, 3 * 16000000)

    def test_firmware_display_interval(self):
        # Boot in accelerated mode, then advance deterministic host time during
        # real firmware execution. No application functions are mocked.
        b = self.b
        b.realtime = False
        for _ in range(500):
            b.run()
            if b.sleeping:
                break
        self.assertTrue(b.sleeping)
        b.realtime = True
        self.wall = b.started + b.now / 16000000
        b.input("touch", True)
        for _ in range(10000):
            self.wall += 0.0001
            b.run(instructions=20000)
            if b.on:
                break
        self.assertTrue(b.on)
        activated = self.wall
        b.input("touch", False)
        levels = []
        for _ in range(40000):
            self.wall += 0.005 if b.waiting else 0.0001
            b.run(instructions=20000)
            levels.append(b.brightness)
            if not b.on:
                break
        duration = self.wall - activated
        self.assertGreaterEqual(duration, 2.98)
        self.assertLess(duration, 3.05)
        self.assertGreater(len(set(levels)), 10)
        self.assertIn(128, levels)
        print(
            f"Firmware display-on interval: {duration:.3f}s; {len(set(levels))} brightness levels"
        )


if __name__ == "__main__":
    unittest.main()
