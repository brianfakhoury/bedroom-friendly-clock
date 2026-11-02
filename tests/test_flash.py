"""Check release integrity and USB selection without touching hardware."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import patch
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))

spec = importlib.util.spec_from_file_location('flash', Path(__file__).parents[1] / 'tools/flash.py')
flash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flash)


class InstallerTests(unittest.TestCase):
    def test_checked_download(self):
        data = b'test firmware'
        metadata = json.dumps({'tag_name': 'v1', 'assets': [
            {'name': 'clock.bin', 'browser_download_url': 'image'},
            {'name': 'SHA256SUMS', 'browser_download_url': 'sums'}]}).encode()
        for digest, valid in [(hashlib.sha256(data).hexdigest(), True), ('0' * 64, False)]:
            with self.subTest(valid=valid), tempfile.TemporaryDirectory() as directory:
                with patch.object(flash, 'download', side_effect=[metadata, data, f'{digest}  clock.bin\n'.encode()]):
                    if valid:
                        self.assertEqual(flash.release_image('latest', Path(directory)).read_bytes(), data)
                    else:
                        with self.assertRaises(ValueError):
                            flash.release_image('latest', Path(directory))
                        self.assertFalse((Path(directory) / 'clock.bin').exists())

    def test_only_espressif_is_selected(self):
        ports = [SimpleNamespace(vid=0x1234, device='unrelated'),
                 SimpleNamespace(vid=0x303A, device='clock')]
        with patch('serial.tools.list_ports.comports', return_value=ports):
            self.assertEqual(flash.select_port(None), 'clock')
        with patch('serial.tools.list_ports.comports', return_value=ports[:1]):
            with self.assertRaises(ValueError):
                flash.select_port(None)


if __name__ == '__main__':
    unittest.main()
