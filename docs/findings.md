# How this clock works: a guided discovery log

## Start with the division of labor

The external PCF85063 counts wall-clock time. The HP CPU wakes to read it, briefly sample gravity, render digits, and run a brightness envelope. Hardware interrupt sources signal buttons, touch, USB, and deadlines. The LP CPU has no program because there is no useful continuous computation to assign it. The LP watchdog is a hardware peripheral, independent of executing code on the LP CPU.

Read `app/main.c`, then `app/clock.c`, then `hal/time.c`. This traces the entire application: collect events, update explicit state, wait until something matters. The time shown is UTC converted to Pacific civil time; the internal timer measures animation intervals, not dates. DST conversion happens automatically under the implemented US rules. Periodic USB correction addresses crystal drift; drift has not yet been measured.

## TOP-off: what finally made it work

Version 0.10.0 powers down TOP while retaining HP's runtime in LP SRAM. Cold initialization lives in ordinary HP SRAM and is discarded. The LP CPU remains unused: retained memory and low-power peripherals do not require an executing LP program.

The useful debugging change was starting with a manufacturer baseline, proving a custom retained stack/entry, then testing one addition to the bare-metal probe at a time. Global pin holds broke USB recovery on the second cycle; holding only required board outputs passed. GPIO routes and safe levels must be restored before releasing those holds. TOP shutdown also resets the USB peripheral clock, which must be restored before accessing USB registers.

A second trap was the USB transmit FIFO: waiting for space in an uncommitted full packet cannot make progress. Committing that packet through the debugger restored logs from a successfully awakened CPU. Silence was an observation failure, not necessarily a sleep failure. Normal wake now avoids logging, and maintenance output remains best effort.

The standalone PMU port exposed a compiler trap before hardware sleep was attempted. Narrow accesses to register bit-fields did not preserve expected timing fields. Register comparison against the working reference detected it; `-fstrict-volatile-bitfields` made the peripheral accesses 32-bit and fixed the readbacks. This compiler flag is a hardware-interface requirement, not a cosmetic optimization.

PMU regulator control is an internal analog-I²C operation, distinct from the external I²C bus used by the display's power chip, RTC and touch controller. Espressif's initialization hands regulators to the PMU state machine and uses eFuse voltage calibration. Copying a few digital power-down bits omitted that initialization. The final small PMU module includes it explicitly.

Two application activation sites are intentional: a real wake starts at `wake_main()`, while a touch already pending during sleep handoff cancels sleep and returns to `run()`. Successful sleep does not return. Both paths consume the triggering touch once.

The final source port passed 20 timer sleep/wake/display cycles, then the shipping touch-only image passed the user's long-idle/touch/rotation/fade check. Those are functional results, not a current measurement or an overnight reliability test. See [the release record](releases/0.10.0/README.md) for evidence and remaining limits.

## “Off” has several meanings

Not calling Wi-Fi code is not the same as powering off its domain. At ROM handoff the radio clock enables were already off, but its power domain was forced on (`0x1c`). Firmware now asserts reset/isolation and requests power-down (`0x23`). This is explicit and repeated at startup, not an assumption about reset defaults. The application has no radio stack, blobs, credentials, or networking commands. These settings are reversible; reflashing remains possible.

Peripheral access clocks and operating clocks are separate. Turning off only the register bus does not establish that a peripheral's internal clock stopped. Version 0.7 gates both for the identified unused peripherals and reports both masks through `POWER`. Conversely, a register that reads zero after its whole domain is off does not independently prove every internal gate changed: the accessible PMU control is the stronger evidence.

A black OLED is not necessarily asleep. Display-off hides pixels while internal circuits can remain active. CO5300 sleep-in also stops converters, scanning, and its oscillator. That saves an unmeasured amount of power at a cost: 120 ms sleep-out plus configuration time before rendering. An immediate tap after shutoff must first allow the previous 120 ms sleep-entry transition to finish. The app's three-second visible interval starts after preparation.

## Schematics beat names and assumptions

The physical device is the 2.16 board. The original 2.06 assumption produced a dark panel. The working 2.16 wiring uses GPIO15 for panel select, GPIO5 for touch interrupt, and ALDO3 for panel reset; reset is not an MCU pin. The panel is 480×480 with no column offset.

Current Waveshare documentation identifies CO5300 and CST9220. Vendor example driver names SH8601 and CST9217 describe compatible interfaces, not successful chip-identification reads. Our attempted panel reads returned all ones, which must not be presented as identification.

The most recent schematic correction: ALDO2 drives the speaker amplifier's CTRL input through R14, rather than supplying touch. Its optional pull-down is unpopulated. Disabling ALDO2 together with PMIC internal output discharge requests amplifier shutdown. The amplifier's main supply is still connected. Earlier notes calling ALDO2 touch-related were wrong and have been corrected.

The codec and audio ADC share I²C with devices we need. Removing their supply can feed current through signal pins. We retain ALDO1 and use their documented software shutdown sequences. Logical shutdown and physical supply removal are different engineering choices.

## Time survives resets, not absent power

CPU resets and firmware flashing leave the external RTC running when its supply is maintained. USB-only unplugging removed that supply on this setup: the oscillator-stop flag made time invalid, and the clock correctly displayed dashes. USB time setting restored it. A high-quality RTC cannot count without energy; backup retention requires a powered backup path. No battery has been connected or tested here.

## Why deeper sleep is still deferred

WFI gates CPU execution between events; it does not perform a complete PMU sleep transition. True chip sleep requires coordinated clock, bias, power, retention, and wake sequencing. ROM handoff tables were not a complete sleep configuration: some active clock-power/bias fields were zero even while the MCU was running. Copying them was not enough.

Two temporary RAM-only PMU probes lost USB response and automatic recovery. The second included extra active-state clock/bias settings and an independently tested LP watchdog. It still failed to recover. This is evidence that the attempted sequence is incomplete, not proof of a specific root cause or that bare-metal sleep is impossible. Both probes left flash untouched; unplugging recovered the saved clock. No such PMU transition is in the release.

The release watchdog did recover an ordinary deliberately stalled CPU in about 8.9 seconds while retaining external RTC time. It is armed during startup and active work, then disarmed for legitimate WFI. This avoids periodic idle feed wakeups but cannot detect a permanently lost wake interrupt. It also does not establish recovery from power-state failures. Its timeout uses an uncalibrated slow RC clock and an eFuse-selected divider: it is a recovery timer, not a precise clock.

## How to interpret the evidence

Host tests establish calendar, geometry, animation, and lifecycle behavior. USB tests establish command processing, timing messages, RTC progression, and readable control settings. Your observations establish actual digits, rotation, fading, touch, and dark standby. Register readback establishes requested hardware configuration. None of these substitutes for battery-input current measurement or an optical latency measurement.

The remaining large question is total standby current. USB/PHY, flash, always-powered board loads, PMIC losses, touch detection, and the non-sleeping MCU infrastructure remain. There is no measured battery lifetime or demonstrated 100 µA result. See the component inventory in [power-audit.md](power-audit.md) before deciding which next change deserves complexity.

## Version 0.8 decision: responsiveness on USB

The user noticed the extra panel wake delay and chose awake-but-display-off standby while USB-powered. We removed the sleep state and timing machinery from the release, keeping fades, rotation, touch, watchdog, and all unrelated power controls. This is a deliberate trade: the delay was observable, while the energy saving has not been measured. If a battery is added, compare battery-input current for awake-dark and sleep before reintroducing the latency. Version 0.7 source and image are preserved in the workspace under `work/fast-display-backup/`.

## Follow-up: the sleep failure became several separate questions

The [controlled investigation](sleep-investigation/README.md) demonstrated PMU sleep without an RTOS or LP program. My original probe used Wi-Fi beacon wake bit 3 instead of timer wake bit 4. The watchdog also needed a sleep configuration that did not pause it; ordinary system reset could recover the clock while USB remained unavailable, whereas the tested chip-reset setting recovered both. This is why “no USB output” was an inadequate definition of a dead CPU.

Touch exposed a different distinction: a level is only true while the electrical signal is asserted; a latched edge remembers a brief pulse. Level-mode trials collected pending touch only after the fallback timer woke the chip. Low-power GPIO5 falling-edge wake and preserving that event for the application produced prompt single taps, confirmed by the user and GPIO-only wake causes before the timer deadline. No LP-core firmware was necessary.

These are functional results, not current measurements. The regular USB clock still uses WFI; the probe’s fallback timer and delayed USB commands are explicit reasons not to turn an experiment into the default power policy. This investigation also invalidated an attempted RTC interrupt test: GPIO18 belongs to the power-button sensing circuit, not RTC_INT. That failed assumption and the invalid log are retained so later readers do not build conclusions on it.


## Version 0.8.1: explicit startup, honest boundaries

Board declarations and initialization now live together in `app/board.h` and `app/board.c`. This remains a single-board program: drivers and HAL code may include the wiring definitions, while main composes the public hardware and application operations without extra forwarding layers.

`unused_peripheral_clocks_disable()` names precisely what it does; gating selected clocks is not proof of complete peripheral power removal or measured savings. SD chip select is latched high before output enable, and audio output levels are latched low before output enable, avoiding startup pulses from the previous latch state. The charging-indicator command now returns failure on a failed transaction or mismatched control-bit readback; board startup reports `ERR LED OFF` on failure. This verifies the control register, not optical darkness.

Retaining the audio supply is a conservative choice, not a proven electrical necessity. Safe unpowered I2C operation of both audio devices has not been established. Software shutdown remains in place; supply removal and residual current remain unverified.

The release and shared RAM-only experiment build successfully. Existing host tests, hardware smoke and USB service checks pass; radio, peripheral-clock and audio shutdown readbacks remain unchanged. No measured power saving is claimed for this cleanup. Evidence: `cleanup-build-test.log`, `cleanup-probe-build.log`, `cleanup-flash.log`, `cleanup-readback.log`, `cleanup-smoke.log`, and `cleanup-service.log`.


## Version 0.8.3: adaptive 12-hour display

Single-digit hours use three digit slots scaled uniformly by 5/4 and centered in the 480-pixel frame (335 by 210 pixels). Two-digit hours and invalid-time dashes use four slots (350 by 168 pixels). Segment edges are scaled before rotation. The renderer remembers the previous size solely to erase old geometry when the size changes; rotation cleanup clears the previous layout before drawing at the new orientation. Pixel-buffer host checks cover bounds, size transitions, invalid-time transitions and complete erasure in all four orientations. Larger digits draw more pixels; optical wake latency and energy cost have not been measured for this layout.


## Packaging audit (2026-11-01)

Current release status, experimental sleep promotion requirements and archive boundaries are consolidated in [packaging.md](packaging.md). Manufacturer/source snapshots now live under `references/` with provenance and checksums. Battery connector naming conflict and unavailable current telemetry are recorded in [hardware.md](hardware.md). Earlier watchdog-disabled/24-hour claims were stale and have been corrected. The LP CPU remains unused: fixing and testing the hardware wake path is preferable to adding a speculative supervisor.

## Retaining power is not retaining peripheral configuration

The 0.9 promotion exposed a distinction missed by the first functional checks: the CPU-off ROM wake path reset GPIO routing and output enables despite retained TOP power. The CPU and USB were alive, but software I²C and panel outputs were disconnected. A status message saying RTC_INVALID therefore did not prove lost time; every I²C peripheral was failing to respond. Comparing cold/wake pin register values made the fault visible. `board_resume()` now restores MCU pin configuration without resetting the external devices. See the 0.9 release record for exact values and qualification status.

The ROM also re-enables UART0 clocks during wake. The unused-clock shutdown policy must be reapplied after wake, not merely at cold startup. Radio force-off remained 0x23 in the observed wake states.

## Where the diagnostic logs come from

The Mac opens the ESP32-C6 built-in USB Serial/JTAG serial port using Python and pyserial. Firmware accesses the USB controller FIFO directly in hal/usb.c; no UART adapter, RTOS logger or wireless telemetry is involved. ROM reset/boot messages and firmware ON/OFF/wake messages share the connection. STATUS reports application state and an external RTC read; POWER reads the explicitly listed control registers and peripheral registers. Host scripts save replies and time the ON/OFF interval.

These are best-effort messages captured while USB is available, not persistent device logs or continuous monitoring. There is no flash log buffer or network reporting. Lost USB can mean lost observation rather than failed execution; this is why physical display checks and reset/wake cause records were needed. Register readbacks establish configuration, not electrical current or optical latency. Detailed temporary pin dumps were removed after diagnosing the wake issue.

## Diagnostic output can affect wake latency

Version 0.9.0 emitted three USB writes before clock activation, then an ON message before the event loop advanced the fade. usb_puts() waits for FIFO space, with a roughly 20 ms polling timeout when space is unavailable. This creates avoidable USB-dependent delay even though logging is best effort. It is a potential delay, not a measured contribution in every wake.

Version 0.9.1 removes the pre-activation wake log and limits ON/OFF messages to USB maintenance. Rendering, orientation sampling, fade timing and sleep configuration are unchanged. The ROM may still print its own reset messages before our wake entry. Firmware STATUS/POWER replies remain available in maintenance. Software preparation timing starts inside clock_activate(), so it excludes the preceding ROM/wake work and does not measure touch-to-light latency.

## WFI versus reset-wake interaction state

WFI resumes the existing event loop. CPU-off wake enters wake_main, activates the display, then starts a fresh loop. Initializing its touch_down flag to false could treat the still-present wake contact as a second activation. The loop now starts with the already-visible activation treated as consumed; pending-touch handoff does the same. This is a software interaction difference introduced by reset-wake, not an inherent requirement of low-power hardware.

Incremental rendering independently reduces unchanged-frame preparation from about 139 ms to 45 ms. Since both changes were tested together, neither number establishes how much of the original perceived regression came from duplicate handling versus other wake work.

## CST9220 low-power modes: research, not an enabled optimization

The clock's touch driver resets the controller and reads reports; it does not explicitly select a low-power mode. Whether the controller firmware automatically reduces scanning while idle remains unverified.

[Espressif's CST9220 driver documentation, version 0.1.1](https://components.espressif.com/components/espressif/esp_lcd_touch_cst9220/versions/0.1.1/readme?language=en) documents D105 sleep and host-initiated wake through D109, with hardware reset as a fallback. It does not establish that a touch generates a wake interrupt in that sleep mode. This is insufficient evidence to use D105 for our touch-only wake requirement.

The older SensorLib bundled with the Waveshare examples defines D10F as LOWPOWER_MODE and D106 as DIS_LOW_POWER_SCAN_MODE, plus LOW_POWER and LPSCAN enum values. However, its setMode implementation has no cases for those enum values. These names are a research lead, not a validated sequence or proof of CST9220 firmware support. The inspected upstream [SensorLib driver](https://github.com/lewisxhe/SensorLib/blob/master/src/touch/TouchDrvCST92xx.cpp) also implements sleep separately from normal touch reporting.

No touch power-mode command was sent during this investigation. Low-power scanning could be a small implementation change if its tap interrupt behavior is confirmed, but savings, latency and automatic-idle behavior are unknown. Do not apply CST9217 current figures to the CST9220 or claim a measured saving without electrical measurements.
