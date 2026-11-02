"""Pinned ESP32-C6 firmware boot prototype (external board devices not yet modeled)."""

import argparse
import hashlib
from pathlib import Path
import platform
import subprocess
import tarfile
import tempfile
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.51.0"
HOSTS = {
    ("Darwin", "arm64"): (
        "aarch64-apple-darwin",
        "ab02524bd286ce84577639731d0e48d623ce9be65e45d9e1246dd220c1476102",
    ),
    ("Linux", "x86_64"): (
        "x86_64-unknown-linux-gnu",
        "2b9e157470edb8b03167996550292c6df8cc6716025ebad8927db950d7931417",
    ),
    ("Linux", "aarch64"): (
        "aarch64-unknown-linux-gnu",
        "3fca854df8d1fc2fc73e37f3a4e9db214166d0e07dfdbd4702019fd9dcc3146f",
    ),
}
BIN = ROOT / ".tools" / "esp-emulator" / "esp-emu"


def setup():
    if BIN.exists():
        version = subprocess.check_output([BIN, "--version"], text=True).strip()
        if version == f"esp-emu {VERSION}":
            return
    target, checksum = HOSTS[(platform.system(), platform.machine())]
    name = f"esp-emu-{VERSION}-{target}"
    url = f"https://github.com/espressif/esp-emulator/releases/download/v{VERSION}/{name}.tar.gz"
    print(f"Installing esp-emu {VERSION} for {target}", flush=True)
    BIN.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=BIN.parent) as directory:
        staging = Path(directory)
        archive = staging / "download.tar.gz"
        with urlopen(url, timeout=60) as response:
            archive.write_bytes(response.read())
        if hashlib.sha256(archive.read_bytes()).hexdigest() != checksum:
            raise ValueError("Emulator archive checksum mismatch")
        with tarfile.open(archive) as bundle:
            bundle.extractall(staging, filter="data")
        binary = staging / name / "esp-emu"
        binary.chmod(0o755)
        binary.replace(BIN)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", action="store_true")
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify firmware reaches READY; not a board qualification",
    )
    parser.add_argument("--seconds", type=int, default=3)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    setup()
    if args.setup:
        return
    command = [
        str(BIN),
        "--chip",
        "esp32c6",
        "--firmware",
        str(ROOT / "build/clock.bin"),
        "--console",
        "usb-jtag",
        "--timeout",
        f"{args.seconds}s",
    ]
    result = subprocess.run(
        command, capture_output=True, text=True, timeout=args.seconds + 30
    )
    output = result.stdout + result.stderr
    (ROOT / "build/emulator-boot.log").write_text(output)
    print(output, end="")
    if result.returncode:
        raise SystemExit(result.returncode)
    if args.check:
        if "READY BEDSIDE CLOCK" not in result.stdout:
            raise SystemExit("FAIL: firmware did not reach READY")
        print("PASS: actual firmware reached READY under CPU emulation.")
    print(
        "STOCK SOC CHECK ONLY: external device failures are expected. Use make emulate for the virtual board."
    )


if __name__ == "__main__":
    main()
