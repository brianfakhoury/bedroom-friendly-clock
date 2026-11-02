#!/usr/bin/env python3
"""Non-destructive USB protocol and timeout test. Does not change RTC time."""
import argparse
from datetime import datetime
import re
import time
import serial
p = argparse.ArgumentParser()
p.add_argument('--port', default='/dev/cu.usbmodem1101')
a = p.parse_args()
with serial.Serial(a.port, 115200, timeout=.01) as s:
    s.reset_input_buffer()
    def reply(cmd, expected, limit=2):
        s.write(cmd)
        end=time.monotonic()+limit
        seen=[]
        while time.monotonic()<end:
            line=s.readline().decode('ascii',errors='replace').strip()
            if line:
                seen.append(line)
                if expected in line:
                    print(line)
                    return line
        raise AssertionError((cmd, expected, seen))
    first=reply(b'STATUS\n','CLOCK UTC')
    assert 'INVALID' not in first
    for bad in [b'TIME 20260230010203\n',b'TIME 20261101160261\n',b'TIME 2026\n']:
        reply(bad,'ERR TIME')
    reply(b'X'*80+b'\n','ERR LENGTH')
    reply(b'DIM 0\n','ERR DIM')
    reply(b'DIM 256\n','ERR DIM')
    s.write(b'STA');time.sleep(.03)
    reply(b'TUS\n','CLOCK UTC')
    reply(b'SHOW\n','ON')
    on=time.monotonic()
    end=on+4
    while time.monotonic()<end:
        if s.readline().strip()==b'OFF':break
    else:raise AssertionError('OFF timeout')
    interval=time.monotonic()-on
    assert 2.95<interval<3.08,interval
    last=reply(b'STATUS\n','CLOCK UTC')
    assert 'DISPLAY OFF' in last and 'INVALID' not in last
    fmt='%Y-%m-%dT%H:%M:%SZ'
    dt=lambda x: datetime.strptime(re.search(r'UTC (\S+)',x)[1],fmt)
    assert 2 <= (dt(last)-dt(first)).total_seconds() <= 8
    print(f'PASS: fragmented USB, malformed commands, RTC advance, auto-off ({interval:.6f}s)')
