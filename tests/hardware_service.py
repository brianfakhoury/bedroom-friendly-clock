#!/usr/bin/env python3
"""Exercise multiple commands per USB packet and deadlines under service traffic."""
import argparse
import time
import serial
p = argparse.ArgumentParser()
p.add_argument('--port', default='/dev/cu.usbmodem1101')
a = p.parse_args()
with serial.Serial(a.port, 115200, timeout=.01) as s:
    s.reset_input_buffer()
    s.write(b'DIM 255\nSTATUS\nDIAG\nACCEL\nSHOW\n')
    seen = []
    end = time.monotonic() + 3
    while time.monotonic() < end:
        line = s.readline().decode().strip()
        if line:
            seen.append(line)
            if line == 'ON':
                break
    assert 'OK DIM' in seen and 'ON' in seen, seen
    assert sum(x.startswith('CLOCK UTC') for x in seen) == 2, seen
    assert any(x.startswith('ACCEL_HEX') for x in seen), seen
    start = time.monotonic()
    next_query = start
    replies = 0
    while time.monotonic() - start < 4:
        if time.monotonic() >= next_query:
            s.write(b'STATUS\n')
            next_query += .05
        line = s.readline().decode().strip()
        if line.startswith('CLOCK UTC'):
            replies += 1
        if line == 'OFF':
            interval = time.monotonic() - start
            assert 2.95 < interval < 3.08, interval
            break
    else:
        raise AssertionError('Display failed to turn off during service traffic')
    assert replies >= 40, replies
    print(f'PASS: batched commands, read-only DIAG, sensor read, {replies} status replies during {interval:.6f}s display interval')
