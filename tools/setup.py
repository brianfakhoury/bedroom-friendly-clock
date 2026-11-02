#!/usr/bin/env python3
"""Install the pinned ESP32-C6 compiler. Python dependencies are managed by uv."""
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
from urllib.request import urlopen

VERSION = '16.1.0_20260609'
HOSTS = {
    ('Darwin', 'arm64'): ('aarch64-apple-darwin', '5f11c929ecc8a978d9492e4e05cf6f607a4563efdb67e972fea28bef2a07b3bf'),
    ('Linux', 'x86_64'): ('x86_64-linux-gnu', '0b029ab31ebc7f74e2c4779077251b21a22cd620669dc4c82e7adc478d403a8d'),
}


def verified(path, checksum):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest() == checksum


def compiler_works(directory):
    for tool in ('gcc', 'ld', 'size', 'objcopy'):
        if not os.access(directory / 'bin' / f'riscv32-esp-elf-{tool}', os.X_OK):
            return False
    try:
        result = subprocess.check_output(
            [directory / 'bin/riscv32-esp-elf-gcc', '--version'], text=True)
        return VERSION in result
    except (OSError, subprocess.CalledProcessError):
        return False


def install(tools, target, checksum):
    tools.mkdir(parents=True, exist_ok=True)
    destination = tools / 'riscv32-esp-elf'
    marker = destination / '.verified-toolchain'
    identity = f'{VERSION} {target} {checksum}\n'
    if marker.exists() and marker.read_text() == identity and compiler_works(destination):
        print('Compiler is ready.')
        return

    archive = tools / f'riscv32-esp-elf-{VERSION}-{target}.tar.xz'
    if not archive.exists():
        url = f'https://github.com/espressif/crosstool-NG/releases/download/esp-{VERSION}/{archive.name}'
        print(f'Downloading compiler for {target}...', flush=True)
        with tempfile.NamedTemporaryFile(dir=tools, delete=False) as temporary:
            partial = Path(temporary.name)
            try:
                with urlopen(url, timeout=60) as response:
                    shutil.copyfileobj(response, temporary)
                temporary.flush()
                if not verified(partial, checksum):
                    raise ValueError('Compiler checksum mismatch.')
                partial.replace(archive)
            finally:
                partial.unlink(missing_ok=True)
    if not verified(archive, checksum):
        archive.unlink()
        raise ValueError('Corrupt compiler archive removed. Run make setup again.')

    # Publish only a fully extracted, checked installation. If interrupted,
    # the next run either uses the old installation or installs from the cache.
    with tempfile.TemporaryDirectory(dir=tools) as staging:
        with tarfile.open(archive) as bundle:
            bundle.extractall(staging, filter='data')
        extracted = Path(staging) / 'riscv32-esp-elf'
        if not compiler_works(extracted):
            raise ValueError('Extracted compiler is incomplete or has the wrong version.')
        (extracted / marker.name).write_text(identity)
        if destination.exists():
            shutil.rmtree(destination)
        extracted.replace(destination)
    print('Compiler installed. Run make, make test, or make flash.')


def main():
    try:
        host = (platform.system(), platform.machine())
        if host not in HOSTS:
            raise ValueError('Supported hosts: Apple Silicon macOS and x86-64 Linux.')
        install(Path(__file__).resolve().parents[1] / '.tools', *HOSTS[host])
    except (OSError, ValueError, tarfile.TarError) as error:
        print(f'Setup failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
