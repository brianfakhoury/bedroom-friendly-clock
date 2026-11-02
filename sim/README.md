# Virtual board prototype

Run `make emulate`, then open **http://127.0.0.1:8080**. Dependencies are pinned
in the `sim` uv group and installed automatically. On a fresh checkout, first
run `make setup` to install the firmware compiler.

Press the display to wake/touch it. KEY sends a momentary press; the firmware
toggles display hold while awake.
BOOT and PWR latch down on click and release on the next click. KEY and BOOT
drive GPIO10/GPIO9 and do not wake standby. Their physical state is separate
from the firmware display-hold badge. PWR uses a simplified PMIC model: four
seconds held turns power off; release then press powers on with a fresh ELF load.
The threshold is a prototype choice, not a calibrated AXP2101 timing model.
RTC backup and physical button positions survive the PWR-button power cycle.
The separate 5V ON/OFF controls simulate supply removal, with no battery modeled:
OFF stops the board and ON cold-loads the ELF. RTC oscillator-stop is then reported
until a valid TIME command sets it. ON while already powered does not reset it.
Holding KEY through the model input API at power-on exercises the firmware's maintenance startup. Holding
BOOT requests ROM download, explicitly stopped as unsupported because this
backend bypasses ROM. Release BOOT and power-cycle to return to the application.
Waveshare documents long-press-off/short-press-on behavior at
https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16. A command reference lists all seven supported USB commands. The inspection
panel shows CPU/PC, timer/GPIO state, and programmed power/clock/watchdog settings.
Power policies are labeled as requests because physical domain gating is not modeled.
Hold is a read-only debugger observation located through ELF symbols and DWARF
member offsets; missing debug information produces an unknown state.

Choose orientation, send USB commands (`STATUS`, `TIME YYYYMMDDhhmmss`, `DIM 255`,
`SHOW`, `POWER`), or disconnect a modeled device. USB and KEY do not wake a
sleeping board. Ctrl-C stops the server. An emulation fault appears in the UI.

`make emulator-check` exercises boot, device initialization, touch wake, pixel
output, RTC read/set, fade/sleep, repeat wake, KEY hold/release, and missing-PMIC
errors. It writes `build/emulator-frame.png` for inspection. To use another port:
`uv run --locked --group sim python sim/virtual_board.py --port 8081`.

## What actually runs

Unicorn executes the normal `build/clock.elf` RV32IMAC instructions, loaded from
its ELF segments at their linked addresses. Firmware startup, HAL, I2C/QSPI bit
banging, drivers, renderer, parser and event loop run unchanged. No application
function is intercepted or replaced. The flash boot ROM is bypassed by loading
the ELF directly at `_start`.

`board.py` supplies a **partial ESP32-C6 MMIO model**, GPIO output/enable/input
state, SYSTIMER snapshots/comparator waiting, USB FIFO and sleep/wake handoff.
I2C devices decode GPIO7/GPIO8 edges, including address ACK/NACK, repeated START,
register pointers and master ACK. The panel decodes GPIO clock/data/CS traffic
into RGB565 pixels; the browser displays a brightness-scaled framebuffer.

Device models cover the firmware-used register behavior of the RTC, touch,
accelerometer, PMIC and audio chips. RTC advances with virtual time and accepts
firmware writes; accelerometer samples follow the orientation control. PMIC and
audio are register stores, not analog power/audio simulations.

## Fidelity limits

This is not a complete ESP32-C6 emulator. Unknown MMIO currently reads stored
values or zero. Clock gating, interrupt routing, GPIO mux selection, pull-up
strength, bus timing/stretching, watchdog expiry and analog behavior are not
validated. WFI suspends execution until modeled input or the programmed timer
boundary. Interactive SYSTIMER and RTC time follow the host monotonic clock;
reads do not advance time and WFI does not skip future deadlines. Automated
scenarios also have an explicitly accelerated mode. Execution is not cycle
accurate; drawing-time diagnostics are not hardware performance measurements.
The browser polls brightness at approximately 60 Hz and fetches the framebuffer
only when pixels change. It shows a square 480 × 480 panel with modest cosmetic
corner rounding, consistent with the Waveshare product photos (the CSS radius
is an approximation, not a measured panel mask).

The two ROM wake-registration calls are modeled at their fixed ABI addresses.
ROM CRC validation and full power-domain reset behavior are not implemented;
wake jumps to the registered firmware assembly entry with retained memory.
These limits mean successful virtual sleep/wake is not physical qualification.
Changes to unmodeled hardware need model extensions and real-board testing.

## Stock emulator investigation

`make emulator-soc-check` retains the separate checksum-pinned Espressif
esp-emu 0.51.0 smoke test. It boots the flash image through its ROM model but
cannot connect our GPIO-driven external peripherals through its documented API.
Its hardware-I2C device option does not serve a bit-banged bus. Espressif QEMU at
`febae182e132e4055529be423a818225ebddaa3a` also has a stub GPIO implementation.
This motivated the extensible Unicorn/MMIO prototype used by `make emulate`.

References:
- https://www.unicorn-engine.org/
- https://github.com/espressif/esp-emulator
- https://github.com/espressif/qemu/tree/febae182e132e4055529be423a818225ebddaa3a
