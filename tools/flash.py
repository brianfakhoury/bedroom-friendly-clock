#!/usr/bin/env python3
"""Install a checked release on the Waveshare ESP32-C6-AMOLED-2.16."""
import argparse
from datetime import datetime, timezone
import time
import serial
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from urllib.request import urlopen
from urllib.error import URLError

from device import select_port

REPO = 'brianfakhoury/bedroom-friendly-clock'


def download(url):
    with urlopen(url, timeout=60) as response:
        return response.read()


def release_image(release, directory):
    endpoint = 'latest' if release == 'latest' else 'tags/' + release
    info = json.loads(download(f'https://api.github.com/repos/{REPO}/releases/{endpoint}'))
    assets = {a['name']: a['browser_download_url'] for a in info['assets']}
    print(f"Downloading {info['tag_name']}...")
    data = download(assets['clock.bin'])
    checksums = download(assets['SHA256SUMS']).decode().splitlines()
    expected = next(line.split()[0] for line in checksums
                    if line.split()[-1].lstrip('*') == 'clock.bin')
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Firmware checksum does not match. No firmware was written.')
    path = directory / 'clock.bin'
    path.write_bytes(data)
    return path


def set_time(port, timeout=30):
    """Wait for the flashed application, then require its RTC-write acknowledgement."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with serial.Serial(port, 115200, timeout=0.2, write_timeout=1) as connection:
                connection.write(b'STATUS\n')
                ready_until = min(deadline, time.monotonic() + 1)
                while time.monotonic() < ready_until:
                    if connection.readline().startswith(b'CLOCK UTC '):
                        break
                else:
                    continue
                command = datetime.now(timezone.utc).strftime('TIME %Y%m%d%H%M%S\n')
                connection.write(command.encode('ascii'))
                reply_until = min(deadline, time.monotonic() + 2)
                while time.monotonic() < reply_until:
                    reply = connection.readline().strip()
                    if reply == b'OK TIME':
                        return
                    if reply == b'ERR TIME':
                        raise ValueError('The clock could not set its RTC.')
        except (OSError, serial.SerialException):
            time.sleep(0.2)
    raise ValueError('No time-setting acknowledgement received. Tap and press KEY, then run make set-time.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='USB serial port; otherwise detect Espressif devices')
    source = parser.add_mutually_exclusive_group()
    source.add_argument('--release', default='latest', help='GitHub release tag (default: latest)')
    source.add_argument('--file', type=Path, help='local firmware image')
    parser.add_argument('--yes', action='store_true', help='skip the replacement confirmation')
    args = parser.parse_args()
    try:
        port = select_port(args.port)
        with tempfile.TemporaryDirectory() as temporary:
            path = args.file or release_image(args.release, Path(temporary))
            if not path.is_file():
                raise ValueError(f'Image not found: {path}')
            print(f'Target: {port}\nBoard: Waveshare ESP32-C6-AMOLED-2.16')
            if not args.yes and input('Replace installed firmware? [y/N] ').lower() != 'y':
                print('Cancelled.')
                return 0
            # esptool checks the chip type and verifies the written flash data.
            subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32c6',
                            '--port', port, '--no-stub', 'write_flash', '0', str(path)], check=True)
            print('Firmware installed. Tap the screen to set the time.', flush=True)
            try:
                set_time(port)
            except ValueError as error:
                print(f'Firmware installed, but time setup failed: {error}', file=sys.stderr)
                return 1
            print('Time set.')
        return 0
    except (OSError, URLError, ValueError, KeyError, StopIteration,
            subprocess.CalledProcessError, ImportError, EOFError) as error:
        print(f'Install failed: {error}', file=sys.stderr)
        print('See README.md for connection and download-mode instructions.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
