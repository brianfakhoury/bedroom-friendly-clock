#!/usr/bin/env python3
"""USB clock control; RTC is UTC, display is America/Los_Angeles."""
import argparse
from datetime import datetime, timezone
import time
import serial
p = argparse.ArgumentParser()
p.add_argument('command', choices=['set-time', 'status', 'show', 'monitor', 'brightness', 'sleep'])
p.add_argument('--port', default='/dev/cu.usbmodem1101')
p.add_argument('--level', type=int, choices=range(1,256), default=128, metavar='1..255')
a = p.parse_args()
with serial.Serial(a.port, 115200, timeout=0.2) as s:
    if a.command == 'set-time':
        cmd = 'TIME ' + datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S')
    elif a.command == 'brightness':
        cmd = f'DIM {a.level}'
    else:
        cmd = {'status': 'STATUS', 'show': 'SHOW', 'monitor': '', 'sleep': 'SLEEP'}[a.command]
    if cmd:
        s.write((cmd + '\n').encode('ascii'))
        s.flush()
    end = time.monotonic() + (5 if a.command == 'show' else 2)
    while a.command == 'monitor' or time.monotonic() < end:
        try:
            data = s.read(4096)
        except (OSError, serial.SerialException):
            if a.command != 'sleep':
                raise
            print('USB disconnected after the sleep request.')
            break
        if data:
            print(data.decode('ascii', errors='replace'), end='', flush=True)
