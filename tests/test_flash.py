"""Check release integrity and USB selection without touching hardware."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import patch, MagicMock
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

    def test_time_setting_requires_application_acknowledgement(self):
        connection = MagicMock()
        connection.__enter__.return_value = connection
        connection.readline.side_effect = [b'boot log\n', b'CLOCK UTC INVALID\n', b'OK TIME\n']
        with patch.object(flash.serial, 'Serial', return_value=connection):
            flash.set_time('clock')
        writes = [call.args[0] for call in connection.write.call_args_list]
        self.assertEqual(writes[0], b'STATUS\n')
        self.assertRegex(writes[1].decode(), r'^TIME \d{14}\n$')

    def test_rtc_failure_is_not_reported_as_success(self):
        connection = MagicMock()
        connection.__enter__.return_value = connection
        connection.readline.side_effect = [b'CLOCK UTC INVALID\n', b'ERR TIME\n']
        with patch.object(flash.serial, 'Serial', return_value=connection):
            with self.assertRaisesRegex(ValueError, 'could not set'):
                flash.set_time('clock')
        with self.assertRaisesRegex(ValueError, 'No time-setting acknowledgement'):
            flash.set_time('clock', timeout=0)

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
