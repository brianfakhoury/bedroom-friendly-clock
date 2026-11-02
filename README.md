# Bedroom-friendly clock

A clock that keeps the room dark. Tap the screen to see the time.

This firmware is for the **Waveshare ESP32-C6-AMOLED-2.16** only.
The 2.06-inch board has different hardware.

## Operation

- The screen shows red digits on a black background.
- The time uses a 12-hour format without AM or PM.
- The digits turn to match the board orientation.
- The display fades in over 180 ms and fades out after about three seconds.
- Default brightness is 50%.
- The external RTC keeps time while the processor sleeps.

The display uses Pacific time, including daylight saving time.
To use another time zone, change `firmware/app/local_time.c`.
Wi-Fi and Bluetooth are disabled. There is no network time service.

## Install firmware

You need Python 3, a USB data cable, and this repository.

1. Connect the board to USB.
2. Tap the screen.
3. Hold BOOT and KEY while the time is visible.
4. Release both buttons.
5. Run the installer:

```sh
python3 -m venv .tools/venv
.tools/venv/bin/python -m pip install -r tools/requirements.txt
.tools/venv/bin/python tools/flash.py
```

The installer downloads the latest release and checks its SHA-256 checksum.
It selects a connected Espressif USB device or asks you to select one.
It asks before it replaces the installed firmware.
Use `--file build/clock.bin` to install a local build.
Use `--release v0.10.2` to select a release.

If USB is unavailable, disconnect the board. Hold KEY when you reconnect it.
Keep KEY held for five seconds. Then release KEY and run the installer again.
For a blank board, hold BOOT when you connect USB to enter the ROM loader.

## Set the time

Enter USB maintenance mode with BOOT and KEY as described above.
Use the port shown by the installer:

```sh
.tools/venv/bin/python tools/clock.py set-time --port /dev/cu.usbmodem1101
.tools/venv/bin/python tools/clock.py sleep --port /dev/cu.usbmodem1101
```

The first command copies your computer's UTC time to the external RTC.
The second command returns the clock to standby.
The example port is for macOS. Your port can be different.
Without battery power, disconnection from USB can erase the RTC time.
Dashes indicate that the clock cannot read valid time.

## Design

The firmware uses C and RISC-V assembly. It has no RTOS, ESP-IDF runtime, or dynamic allocation.
The HP processor handles input, orientation, and drawing while the display is active.
In standby, the HP CPU and TOP domain are off. The LP CPU does not run.
Retained LP memory contains the wake code and application state.
The touch interrupt wakes the processor through the LP GPIO hardware.

The panel stays powered with its display off. This reduces display wake delay.
The external RTC and touch controller also stay powered.
Unused radio hardware is off and isolated.
Some other board supplies remain on; see the power audit.

| Directory | Purpose |
| --- | --- |
| `firmware/app` | Clock behavior, board setup, and USB commands |
| `firmware/drivers` | External device control |
| `firmware/hal` | MCU registers, clocks, buses, and power |
| `firmware/boot` | Startup, wake entry, and memory layout |
| `tests` | Host tests for time, drawing, orientation, and animation |
| `docs` | Design, hardware references, and test history |

## Build and test

The setup script supports Apple Silicon macOS and x86-64 Linux.
It installs a pinned compiler and Python tools into `.tools`.
You also need `make` and a host C compiler.

```sh
./tools/bootstrap.sh
make
make test
```

GitHub Actions builds and tests each push and pull request.
Version tags create releases with the firmware, debug file, memory map, and checksums.

## Validation and limits

Touch wake, rotation, fades, and repeated deep sleep have passed device checks.
Host tests check time conversion, animation, orientation, and pixel rendering.
Automated tests cannot verify electrical behavior or visible wake delay.
Battery current, battery life, and long-duration reliability remain unmeasured.

Read the [architecture](docs/architecture.md), [power audit](docs/power-audit.md),
[findings](docs/findings.md), and [hardware references](docs/references/README.md).
Older investigation records describe experiments, including failed configurations.
They do not all describe the current firmware.

## License

Original project code uses the MIT license.
Espressif-derived code retains its [Espressif license](firmware/hal/ESPRESSIF-LICENSE).
Vendor documents and reference sources retain their own terms.
