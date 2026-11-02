# 0.10.0 — TOP-off standby, strict bare-metal

Installed, verified, and returned to normal touch-only standby. The host received `OK SLEEP` and the clock USB interface disconnected as expected; `standby-return.log` records it. `tools/clock.py sleep` now flushes its request and handles that expected disconnect cleanly.

Installed and verified on the Waveshare ESP32-C6-AMOLED-2.16, ESP32-C6 revision 0.2. The final image passed single-tap wake after at least 30 seconds dark, repeated taps, rotation and fades: the user's report was “looks perfect!” USB maintenance was then entered successfully and the final host checks passed.

## Implementation

HP TOP, AON, CPU, PLL/crystal and VDD_SPI are requested off during standby. Retained LP SRAM holds the runtime code, digit cache and application state; LP GPIO and RC_FAST remain powered for the proven falling-edge touch latch. The LP CPU does not execute. There is no periodic standby timer. The panel remains awake but display-off, preserving pixels and response speed. This is a qualified TOP-off policy, not a claim of the C6's minimum possible board current.

Cold-only board, display and PMU initialization live in HP SRAM and are discarded when TOP powers down. Wake/runtime code stays in LP SRAM. The linked retained allocation ends at `0x50002990`: 10,640 bytes allocated, plus the unchanged 4 KiB stack reservation and 1,648 bytes margin. The total linked code/constant size is 13,914 bytes. No heap, RTOS, ESP-IDF runtime, radio firmware, or unresolved link symbols are present.

`hal/pmu.c` is a focused adaptation of Espressif's v5.4.2 hardware initialization, including regulator control handoff, eFuse voltage calibration and PMU state/transition settings. Its standalone register header and license are shipped locally. The normal build does not consume the SDK checkout or prebuilt SDK objects. The compiler must use `-fstrict-volatile-bitfields` so MMIO bit-fields use 32-bit accesses.

The wake path restores the USB peripheral clock before the event/service code accesses USB. Sleep holds only required board outputs using LP_AON per-pin holds; it excludes USB and wake inputs. GPIO routes/levels are restored before those holds are released. USB disconnects during standby and reconnects on wake. A 100 ms disconnect interval occurs after the screen has gone dark, before sleep; it is not on the activation path.

## Evidence

- The standalone reference probe passed five TOP-off wakes; staged radio, clock, board, display, touch initialization and normal wake-handler additions passed repeated timer wakes. Global pin holds caused lost USB communication on the second wake. Per-pin holds passed; absence of USB alone was never taken as proof of a dead CPU.
- The integrated reference-initialization clock passed repeated timer wakes and user-confirmed touch-only wakes before the source port.
- The standalone source port's PMU state/transition register readbacks match the passing reference, except the unused modem scheduling target, intentionally zero. The first comparison caught missing XTAL/minimum-sleep fields from narrow compiler stores; enforcing 32-bit volatile bit-fields fixed the discrepancy before sleep testing.
- A timer qualification variant of the standalone clock passed **20 consecutive sleep/wake/display cycles** with valid advancing RTC time. It returns to maintenance after each timer wake so the host verifies the activation count. The shipped image removes that timer and test maintenance behavior.
- Final debug readback: HP sleep power `0xe8200000`; LP sleep crystal request zero, LP peripheral/RC_FAST retained; timer target enable zero.
- Final flash hash verified by esptool. Final touch-only image physically checked by the user, including rotation, fades and maintenance entry.
- All existing RTC/DST, animation, orientation, lifecycle and incremental-renderer host tests passed.
- Final USB smoke passed malformed/fragmented commands, RTC advance and shutoff at **3.000600 s**. Batched service passed 60 status replies during **3.002476 s** without extending display duration.
- Post-wake `POWER`: radio force control `0x23`; both unused-clock masks zero; PMIC DC/LDO enables `0x01/0x05/0x00`; audio shutdown values and accelerometer-off state verified. Raw logs are alongside this file.
- Unchanged-digit preparation during the 20-cycle test measured `0xd8fa` = **55,546 µs**. This includes orientation/RTC/rendering and excludes hardware wake and fade-to-visible time. Changed digits and rotations take longer. The user found final response satisfactory; optical wake latency was not measured.

## Limits and recovery

Current and battery life are unmeasured. No overnight/multi-day soak or temperature-range qualification has been completed. PMU transition timing currently uses the nominal RC periods from the passing probe; this is separate from eFuse voltage calibration and the external RTC's timekeeping. Calibrating those periods and validating voltage/temperature extremes remain future hardening work.

The active-work watchdog pauses during standby and cannot diagnose an indefinitely lost wake input. Hold KEY during cold startup for maintenance, or tap then hold BOOT+KEY while visible. Normal standby supports touch, not BOOT/KEY wake. USB-only power loss invalidates RTC time. If recovery is necessary, the previous CPU-off 0.9.1 image is preserved separately; it is not overwritten by this release.

Toolchain: Espressif GCC 16.1.0 (`esp-16.1.0_20260609`), esptool 4.12.0. Build with `make`; run host checks with `make test`. Hardware checks require maintenance mode.

Final `clock.bin` SHA-256: `b139dc2ec457cf79e2725fdda28830da539a2f249b731ce23e62df3836ea5fe0`.
