# Architecture

Time passes without software running. The external RTC owns UTC; firmware presents it when requested. There is one HP-core event loop, one presentation state, no heap, no RTOS, and no LP-core program. Wi-Fi is out of scope.

## Version 0.10 standby

Runtime code, constants and clock state reside in the C6's 16 KiB retained LP SRAM, with 4 KiB reserved for stack and checked by the linker. Cold-only PMU, board and panel initialization live in HP SRAM; they are never called after wake. LP memory is used by HP; the LP CPU does not run. ROM validates the immutable retained region before entering `boot/wake.S`, which establishes a fresh stack/global pointer and calls `wake_main`. Mutable state and the digit cache survive independently of the discarded stack.

`hal/pmu.c` initializes regulator control, eFuse voltage calibration and hardware power-state tables once during cold board startup. It is a small standalone adaptation of the manufacturer's hardware code, with no SDK runtime. `hal/standby.c` then arms the LP GPIO5 falling-edge latch, registers the retained entry, holds required output pins and requests TOP/AON/CPU shutdown. PLL/crystal are off in standby; LP SRAM, LP GPIO and RC_FAST remain available. There is no timer wake or LP-core program.

Wake restores timers, the USB peripheral clock and touch pin routing. `board_resume()` restores GPIO routes and safe output levels before releasing per-pin holds, then restores software I²C. It does not reset the panel or touch controller. The application reads the RTC, samples orientation and resumes its existing renderer. The LP watchdog pauses during legitimate sleep and protects active work; it cannot detect a permanently lost wake input.

Cold boot enters standby unless KEY is held. While visible, BOOT+KEY enters USB maintenance; SLEEP leaves it. USB disconnects in standby and reconnects on wake. WFI remains the wait mechanism during animation and maintenance. BOOT/KEY are not supported standby wake inputs.

## Read the code in this order

1. [`app/main.c`](../firmware/app/main.c) connects activation events, USB servicing, presentation updates, and waiting. It owns the clock state and touch/button interaction state.
2. [`app/clock.c`](../firmware/app/clock.c) owns the presentation lifecycle: sample orientation, read UTC, prepare digits, fade, hold, hide. Its explicit `clock_state` contains presentation state, not a second wall clock. It has no USB parsing or register access.
3. [`app/animation.c`](../firmware/app/animation.c), [`app/orientation.c`](../firmware/app/orientation.c), and [`app/local_time.c`](../firmware/app/local_time.c) contain deterministic calculations. Pacific daylight-saving policy and service date formatting are outside the RTC driver.
4. [`app/digits.c`](../firmware/app/digits.c) owns the seven-segment shapes and placement. Orientation and ink are explicit arguments. Erasing uses the same geometry with black ink, so rotated images cannot leave old segments behind.
5. [`drivers/display.c`](../firmware/drivers/display.c) knows rectangles, brightness, display on/off, and panel initialization. It does not know what time is or how digits look. Other drivers own the RTC, touch, accelerometer, and PMIC protocols.
6. [`hal/time.c`](../firmware/hal/time.c) owns the SYSTIMER and pending-event wait. [`app/board.h`](../firmware/app/board.h) names the board wiring; [`app/board.c`](../firmware/app/board.c) composes device startup. GPIO, I²C, and USB mechanisms have separate small source files.
7. [`app/service.c`](../firmware/app/service.c) parses maintenance commands. It requests activation through its return value, so USB SHOW, touch, and buttons use the same application activation path. DIAG reports status without changing hardware.

## Lifecycle and invariants

An activation samples acceleration, retaining the previous orientation on failure or ambiguity. A changed orientation hides and erases old segments. The RTC is read afresh; invalid time produces dashes. The prepared image fades in over 180 ms, holds for 2470 ms, then fades out over 350 ms. The panel receives display-off at the end. A repeat activation restarts the interval from the current brightness unless the orientation changed.

The interval starts after pixel preparation. Timer deadlines are absolute and tied to that activation; maintenance work cannot reschedule the hold for another full interval. Late work is caught up on the next update. Commands and device transactions still take finite processing time, so this is not a hard real-time guarantee against arbitrarily slow hardware or a failed timer.

The active/maintenance event wait accepts zero for an indefinite WFI wait. Otherwise it programs one SYSTIMER comparator. GPIO/USB status stays latched through the check-to-WFI boundary. USB FIFO availability is checked before waiting so partially consumed packets cannot strand queued commands. Service input consumes at most one line or 64 bytes per event-loop pass.

The renderer retains four seven-segment masks and the ink color alongside the application in LP SRAM. It writes only changed segments; geometry changes erase the old layout first. The panel retains its pixels while display-off, so unchanged digits need no transfer. This cache assumes the panel is initialized only at cold boot.

No tick runs during standby. During fades, one-shot deadlines are roughly 10 ms apart. During the hold, only the next fade boundary is scheduled. The panel retains its pixels; the CPU does not refresh them. The accelerometer is disabled after each sample and the gyroscope stays off.

## Intentional limits

- **Sleep:** Normal dark standby uses TOP-off. Board current, temperature-range behavior and long-duration reliability remain unmeasured.
- **Buses:** I²C and QSPI are still software-driven. I²C traffic is small and infrequent; pixel transfer is the stronger candidate for hardware SPI. The panel fast path explicitly asserts its dependency on contiguous GPIO0–4.
- **Fault recovery:** the LP watchdog protects startup and active work, and is disarmed for legitimate WFI. It cannot detect indefinitely lost wake events. The release has passed functional checks but has not completed a multi-day unattended soak test.
- **USB:** service output is bounded best effort, not a persistent log. Normal wake does not print diagnostics; enter maintenance for host checks.

The PMU handles regulator handoff, voltage bias, power domains, clock transitions and timing. The standalone implementation deliberately supports one sleep policy. Its nominal RC transition periods are separate from wall time; calibration of these periods and temperature-range testing remain future hardening work. Hardware SPI is also deferred pending evidence that it is needed.

`activate()` has two legitimate callers in the sleep flow: `wake_main()` after an actual reset-wake, and `run()` when `standby_enter()` cancels sleep for a touch already pending during pin handoff. Successful sleep never returns to that call. One physical wake event is marked consumed before the event loop accepts another touch.
