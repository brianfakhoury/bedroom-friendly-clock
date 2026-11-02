#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
case "$(uname -s)-$(uname -m)" in
 Darwin-arm64)
  target=aarch64-apple-darwin
  checksum=5f11c929ecc8a978d9492e4e05cf6f607a4563efdb67e972fea28bef2a07b3bf ;;
 Linux-x86_64)
  target=x86_64-linux-gnu
  checksum=0b029ab31ebc7f74e2c4779077251b21a22cd620669dc4c82e7adc478d403a8d ;;
 *) echo 'Supported hosts: Apple Silicon macOS and x86-64 Linux.' >&2; exit 1;;
esac
mkdir -p .tools
python3 -m venv .tools/venv
.tools/venv/bin/python -m pip install -r tools/requirements.txt
archive=.tools/riscv32-esp-elf.tar.xz
url=https://github.com/espressif/crosstool-NG/releases/download/esp-16.1.0_20260609/riscv32-esp-elf-16.1.0_20260609-$target.tar.xz
[ -f "$archive" ] || curl -fL "$url" -o "$archive"
printf '%s  %s\n' "$checksum" "$archive" | shasum -a 256 -c -
[ -d .tools/riscv32-esp-elf ] || tar -xf "$archive" -C .tools
printf 'Ready. Run make and make test. Use tools/flash.py to install.\n'
