# Bedroom-friendly clock

A touch-activated bedside clock for a dark and silent bedroom. This project uses a [$40 dev kit](https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm) and bare-metal firmware to make a digital clock that just does what I need.

## Operation

- The screen shows red digits on a black background.
- The time uses a 12-hour format without AM or PM.
- The digits turn to match the board orientation.
- The display fades in over 180 ms and fades out after about three seconds.
- Default brightness is 50%.
- The external RTC keeps time while the processor sleeps.

The display uses Pacific time, including daylight saving time. To use another time zone, change `src/app/local_time.c`. Wi-Fi and Bluetooth are disabled. There is no network time service.

## Install firmware

Install [uv](https://docs.astral.sh/uv/getting-started/installation/) and `make`, and connect a USB data cable. These commands are for macOS and Linux.

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
make flash-release
```

The installer downloads the latest release and checks its SHA-256 checksum. It selects a connected Espressif USB device or asks you to select one. Confirm installation. After the clock restarts, tap the screen when prompted. The installer sets the time automatically.

The installer sets the RTC from your computer automatically. Use `make flash` to build and install local source after `make setup`. Use `make flash-release RELEASE=v0.11.1` to select a release.

## Adjust the time

Tap the screen, then press KEY to keep the display and USB awake. If the clock has no valid time, it shows dashes. You can also hold KEY while connecting power to start with the display held on.

Set the time:

```sh
make set-time
```

This command copies your computer's UTC time to the external RTC. Press KEY again to return to normal operation. The tool detects connected Espressif USB devices. If several are connected, select one from the list. Add `PORT=your-port` to select a port explicitly. Without battery power, disconnection from USB can erase the RTC time. Dashes indicate that the clock cannot read valid time.

## Update an installed clock

Tap the screen, then press KEY to keep the display and USB awake. Run `make flash-release` and follow its KEY instructions.

## Design

The firmware uses C and RISC-V assembly. It has no RTOS, ESP-IDF runtime, or dynamic allocation. The HP processor handles input, orientation, and drawing while the display is active. In standby, the HP CPU and TOP domain are off. The LP CPU does not run. Retained LP memory contains the wake code and application state. The touch interrupt wakes the processor through the LP GPIO hardware.

The panel stays powered with its display off. This reduces display wake delay. The external RTC and touch controller also stay powered. Unused radio hardware is off and isolated. Some other board supplies remain on; see the power audit.

| Directory | Purpose |
| --- | --- |
| `src/app` | Clock behavior and USB commands |
| `src/board` | Wiring, supplies, and device startup |
| `src/drivers` | External device control |
| `src/hal` | MCU registers, clocks, buses, and power |
| `src/boot` | Startup, wake entry, and memory layout |
| `tests` | Host tests for time, drawing, orientation, and animation |
| `docs` | Design, hardware references, and validation limits |

## Build and test

`make setup` supports Apple Silicon macOS and x86-64 Linux. It installs a pinned compiler into `.tools`. uv manages Python and the locked dependencies. You also need `make` and a host C compiler.

```sh
make setup
make
make test
make flash
```

GitHub Actions builds and tests each push and pull request. Version tags create releases with the firmware, debug file, memory map, and checksums.

## Validation and limits

Touch wake, rotation, fades, and repeated deep sleep have passed device checks. Host tests check time conversion, animation, orientation, and pixel rendering. Automated tests cannot verify electrical behavior or visible wake delay. Battery current, battery life, and long-duration reliability remain unmeasured.

Read the [architecture](docs/architecture.md), [power audit](docs/power-audit.md), and [hardware references](docs/references/README.md).

## License

Original project code uses the MIT license. Espressif-derived code retains its [Espressif license](src/hal/ESPRESSIF-LICENSE). Vendor documents and reference sources retain their own terms.
