"""Setup must reject corrupt downloads and recover incomplete installations."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('toolchain_setup', Path(__file__).parents[1] / 'tools/setup.py')
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class SetupTests(unittest.TestCase):
    def test_corrupt_archive_removed_without_replacing_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            tools = Path(folder)
            destination = tools / 'riscv32-esp-elf'
            destination.mkdir()
            (destination / 'old').touch()
            archive = tools / f'riscv32-esp-elf-{setup.VERSION}-test.tar.xz'
            archive.write_bytes(b'corrupt')
            with self.assertRaises(ValueError):
                setup.install(tools, 'test', '0' * 64)
            self.assertFalse(archive.exists())
            self.assertTrue((destination / 'old').exists())

    def test_incomplete_install_replaced_and_repeat_skipped(self):
        with tempfile.TemporaryDirectory() as folder:
            tools = Path(folder)
            destination = tools / 'riscv32-esp-elf'
            destination.mkdir()
            (destination / 'partial').touch()
            archive = tools / f'riscv32-esp-elf-{setup.VERSION}-test.tar.xz'
            with tarfile.open(archive, 'w:xz') as bundle:
                item = tarfile.TarInfo('riscv32-esp-elf/complete')
                item.size = 2
                bundle.addfile(item, io.BytesIO(b'ok'))
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            with patch.object(setup, 'compiler_works', return_value=True):
                setup.install(tools, 'test', digest)
                self.assertFalse((destination / 'partial').exists())
                self.assertTrue((destination / 'complete').exists())
                with patch.object(setup.tarfile, 'open', side_effect=AssertionError('unexpected extraction')):
                    setup.install(tools, 'test', digest)

    def test_wrong_compiler_version_is_rejected(self):
        with patch.object(setup.os, 'access', return_value=True):
            with patch.object(setup.subprocess, 'check_output', return_value='gcc 14.2.0'):
                self.assertFalse(setup.compiler_works(Path('unused')))


if __name__ == '__main__':
    unittest.main()
