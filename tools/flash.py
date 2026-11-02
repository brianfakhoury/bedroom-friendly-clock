#!/usr/bin/env python3
"""Install a checked release on the Waveshare ESP32-C6-AMOLED-2.16."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from urllib.request import urlopen
from urllib.error import URLError

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


def select_port(requested):
    from serial.tools.list_ports import comports
    if requested:
        return requested
    ports = [p for p in comports() if p.vid == 0x303A]
    if not ports:
        raise ValueError('No Espressif USB device found. Enter maintenance mode, or use --port PORT.')
    if len(ports) == 1:
        return ports[0].device
    for i, port in enumerate(ports, 1):
        print(f'{i}: {port.device} ({port.description})')
    choice = int(input('Select device number: '))
    if not 1 <= choice <= len(ports):
        raise ValueError('Invalid device number.')
    return ports[choice - 1].device


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
            print('Firmware installed. Enter maintenance mode to set the time.')
        return 0
    except (OSError, URLError, ValueError, KeyError, StopIteration,
            subprocess.CalledProcessError, ImportError, EOFError) as error:
        print(f'Install failed: {error}', file=sys.stderr)
        print('Check the cable and maintenance mode. See README.md for recovery.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
