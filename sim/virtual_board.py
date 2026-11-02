"""Interactive virtual board: actual firmware ELF, modeled C6 registers and devices."""

import argparse
import json
from pathlib import Path
from queue import SimpleQueue
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
import zlib
from board import Board
from unicorn.riscv_const import UC_RISCV_REG_PC
from inspection import snapshot

ROOT = Path(__file__).resolve().parents[1]


def png(board, scaled=True):
    scale = (board.brightness if board.on else 0) if scaled else 255
    pixels = (
        bytes(board.panel)
        if scale == 255
        else bytes(v * scale // 255 for v in board.panel)
    )
    raw = b"".join(b"\0" + pixels[y * 1440 : (y + 1) * 1440] for y in range(480))

    def chunk(kind, data):
        return (
            struct.pack(">I", len(data))
            + kind
            + data
            + struct.pack(">I", zlib.crc32(kind + data))
        )

    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 480, 480, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 1))
        + chunk(b"IEND", b"")
    )


def check(elf):
    b = Board(elf)

    def until(predicate, limit=500):
        for _ in range(limit):
            b.run()
            if predicate():
                return
        raise AssertionError(
            f"Firmware did not reach expected state; PC={b.cpu.reg_read(UC_RISCV_REG_PC):08x}\n{b.console}"
        )

    until(lambda: b.sleeping)
    assert "READY BEDSIDE CLOCK" in b.console and "ERR" not in b.console
    assert b.pixels == 480 * 480
    assert b.held is False
    info = snapshot(b)
    assert info["Execution (modeled)"]["HP CPU"] == "deep sleep"
    assert info["Sleep power policy (programmed)"]["HP CPU"] == "power-down requested"
    assert (
        info["Clock gates (programmed, not enforced)"]["Hardware I2C APB"] == "disabled"
    )
    b.input("touch", True)
    until(lambda: b.on and b.brightness >= 100)
    assert any(b.panel)
    b.input("touch", False)
    b.input("serial", "STATUS")
    until(lambda: "CLOCK UTC " in b.console)
    assert "INVALID" not in b.console
    b.input("serial", "TIME 20261102120000")
    until(lambda: "OK TIME" in b.console)
    until(lambda: "CLOCK UTC 2026-11-02T12:00:" in b.console)
    until(lambda: b.sleeping)
    assert not b.on
    b.input("rotate", 1)
    b.input("touch", True)
    until(lambda: b.on and b.brightness >= 100)
    (ROOT / "build/emulator-frame.png").write_bytes(png(b))
    b.input("touch", False)
    b.input("key")
    until(lambda: b.waiting)
    # Hold survives the normal three-second display interval.
    b.now += 4 * 16000000
    for _ in range(30):
        b.run()
    assert b.on and not b.sleeping
    assert b.held is True
    b.input("key")
    until(lambda: b.sleeping)
    assert b.held is False
    missing = Board(elf)
    del missing.devices[0x34]
    for _ in range(100):
        missing.run()
        if missing.sleeping:
            break
    assert "ERR PANEL INIT" in missing.console and "ERR LED OFF" in missing.console
    print(
        "PASS: ELF boot, device initialization, GPIO buses, touch wake, pixels, RTC read/set, fade/sleep, rotation, KEY hold/release, missing PMIC."
    )
    print(b.console)


def validate_input(event):
    if not isinstance(event, dict):
        raise ValueError("Expected an input object")
    kind = event.get("kind")
    value = event.get("value")
    if kind not in (
        "touch",
        "key",
        "boot",
        "pwr",
        "supply",
        "rotate",
        "serial",
        "fault",
    ):
        raise ValueError("Unknown input")
    if kind == "fault" and (
        not isinstance(value, dict)
        or value.get("address") not in (0x51, 0x6B, 0x5A)
        or not isinstance(value.get("enabled"), bool)
    ):
        raise ValueError("Invalid device fault")
    if kind == "key" and value is not None and not isinstance(value, bool):
        raise ValueError("Expected boolean")
    if kind in ("touch", "supply", "boot", "pwr") and not isinstance(value, bool):
        raise ValueError("Expected boolean")
    if kind == "rotate" and (type(value) is not int or value not in (0, 1, 2, 3)):
        raise ValueError("Invalid rotation")
    if kind == "serial" and (
        not isinstance(value, str)
        or not value.isascii()
        or len(value) > 47
        or any(ord(c) < 32 or ord(c) == 127 for c in value)
    ):
        raise ValueError("Invalid command")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check", action="store_true")
    p.add_argument("--port", type=int, default=8080)
    args = p.parse_args()
    elf = ROOT / "build/clock.elf"
    if args.check:
        check(elf)
        return
    board = Board(elf, realtime=True)
    inputs = SimpleQueue()
    state = {"status": "Starting firmware", "console": "", "error": None}
    frame = [png(board, scaled=False)]
    inspection = [snapshot(board)]
    stop = threading.Event()

    def worker():
        try:
            last_pixels = -1
            last_frame = 0.0
            revision = 0
            last_inspection = 0.0
            while not stop.is_set():
                while not inputs.empty():
                    event = inputs.get()
                    board.input(event["kind"], event.get("value"))
                board.run(instructions=20000)
                state.update(
                    status="Sleeping — tap to wake" if board.sleeping else "Running",
                    console=board.console,
                    brightness=board.brightness,
                    on=board.on,
                    held=board.held,
                    sleeping=board.sleeping,
                    key_down=board.key_down,
                    boot_down=board.boot_down,
                    pwr_down=board.pwr_down,
                    powered=board.powered,
                    supply_on=board.supply_on,
                    bootloader=board.bootloader,
                    transactions=board.transactions,
                    pixels=board.pixels,
                )
                if (
                    board.pixels != last_pixels
                    and time.monotonic() - last_frame >= 1 / 30
                ):
                    frame[0] = png(board, scaled=False)
                    last_pixels = board.pixels
                    last_frame = time.monotonic()
                    revision += 1
                    state["frame"] = revision
                if time.monotonic() - last_inspection >= 0.25:
                    inspection[0] = snapshot(board)
                    last_inspection = time.monotonic()
                stop.wait(0.001)
        except Exception as exc:
            state.update(status="Emulation stopped", error=str(exc))

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            if self.path == "/state":
                data = json.dumps(state).encode()
                mime = "application/json"
            elif self.path == "/inspect":
                data = json.dumps(inspection[0]).encode()
                mime = "application/json"
            elif self.path.startswith("/frame"):
                data = frame[0]
                mime = "image/png"
            elif self.path == "/":
                data = (ROOT / "sim/index.html").read_bytes()
                mime = "text/html"
            elif self.path == "/ui.css":
                data = (ROOT / "sim/ui.css").read_bytes()
                mime = "text/css"
            elif self.path == "/ui.js":
                data = (ROOT / "sim/ui.js").read_bytes()
                mime = "text/javascript"
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", mime)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_POST(self):
            if self.path != "/input":
                self.send_error(404)
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 1024:
                    raise ValueError("Invalid input size")
                event = json.loads(self.rfile.read(length))
                validate_input(event)
                inputs.put(event)
            except (ValueError, TypeError):
                self.send_error(400)
                return
            self.send_response(204)
            self.end_headers()

    server = HTTPServer(("127.0.0.1", args.port), Handler)
    thread = threading.Thread(target=worker, daemon=True)
    thread.start()
    print(f"Virtual board: http://127.0.0.1:{args.port} (Ctrl-C to stop)", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    main()
