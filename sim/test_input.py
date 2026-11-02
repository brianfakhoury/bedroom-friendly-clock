"""Reject malformed browser inputs before they reach the emulation worker."""

import unittest

from virtual_board import validate_input


class InputTests(unittest.TestCase):
    def test_valid_inputs(self):
        for event in [
            {"kind": "key"},
            {"kind": "touch", "value": False},
            {"kind": "boot", "value": True},
            {"kind": "pwr", "value": False},
            {"kind": "supply", "value": True},
            {"kind": "rotate", "value": 3},
            {"kind": "serial", "value": "TIME 20261102120000"},
            {"kind": "fault", "value": {"address": 0x51, "enabled": True}},
        ]:
            with self.subTest(event=event):
                validate_input(event)

    def test_malformed_inputs(self):
        for event in [
            [],
            None,
            "key",
            {},
            {"kind": "unknown"},
            {"kind": "boot"},
            {"kind": "pwr"},
            {"kind": "key", "value": "false"},
            {"kind": "touch", "value": 1},
            {"kind": "supply", "value": None},
            {"kind": "rotate", "value": True},
            {"kind": "rotate", "value": 1.0},
            {"kind": "rotate", "value": 4},
            {"kind": "serial", "value": "STATUS\nSHOW"},
            {"kind": "serial", "value": "x" * 48},
            {"kind": "serial", "value": "é"},
            {"kind": "fault", "value": {"address": 0x34, "enabled": True}},
            {"kind": "fault", "value": []},
        ]:
            with self.subTest(event=event), self.assertRaises(ValueError):
                validate_input(event)
