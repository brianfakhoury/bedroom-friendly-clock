# Bedroom-friendly clock

A touch-activated bedside clock for the **Waveshare ESP32-C6-AMOLED-2.16**.
The screen stays dark until you tap it.

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

Install [uv](https://docs.astral.sh/uv/getting-started/installation/) and connect a USB data cable.
These commands are for macOS and Linux.

Download this repository:

```sh
git clone https://github.com/brianfakhoury/bedroom-friendly-clock.git
cd bedroom-friendly-clock
```

Put the board in download mode:

1. Disconnect the board from power, including any battery.
2. Hold BOOT.
3. Connect the USB cable while you hold BOOT.
4. Release BOOT.

Run the installer:

```sh
uv run tools/flash.py
```

The installer downloads the latest release and checks its SHA-256 checksum.
It selects a connected Espressif USB device or asks you to select one.
Confirm the device to replace its installed firmware.

After installation, set the time with the instructions below.
Use `--file build/clock.bin` to install a local build.
Use `--release v0.11.0` to select a release.

## Set the time

Tap the screen, then press KEY to keep the display and USB awake.
If the clock has no valid time, it shows dashes.
You can also hold KEY while connecting power to start with the display held on.

Set the time, then return the clock to standby:

```sh
uv run tools/clock.py set-time
uv run tools/clock.py sleep
```

The first command copies your computer's UTC time to the external RTC.
The second command releases the display; it fades out and returns to standby.
You can press KEY again instead.
Both tools detect connected Espressif USB devices.
If several are connected, select one from the list. Use `--port PORT` to select a port explicitly.
Without battery power, disconnection from USB can erase the RTC time.
Dashes indicate that the clock cannot read valid time.

## Update an installed clock

For release 0.10.2 and earlier, hold BOOT and KEY together while the display is visible to enable USB.

Tap the screen, then press KEY to keep the display and USB awake.
Run `uv run tools/flash.py`.
Press KEY again to return to normal operation.

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
| `docs` | Design, hardware references, and validation limits |

## Build and test

The setup script supports Apple Silicon macOS and x86-64 Linux.
It installs a pinned compiler into `.tools`.
uv manages Python and the locked dependencies.
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
and [hardware references](docs/references/README.md).

## License

Original project code uses the MIT license.
Espressif-derived code retains its [Espressif license](firmware/hal/ESPRESSIF-LICENSE).
Vendor documents and reference sources retain their own terms.
