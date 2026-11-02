# Release 0.9.0 — HP CPU-off standby

Installed and functionally checked on the Waveshare ESP32-C6-AMOLED-2.16 on 2026-11-01. Normal dark standby powers down HP CPU; touch wakes through the retained LP SRAM entry. TOP/AON, PLL/crystal and supply settings remain retained. The LP CPU is inactive. No periodic wake or recovery timer is enabled.

## Verification

- Host tests pass: RTC/calendar/DST, fades, orientation, clock lifecycle and adaptive 12-hour digits.
- User confirmed four separate touch activations with correct time and fades. Serial diagnostics record sleep-reset reason 0x5 and GPIO wake cause 0x4.
- USB maintenance entered successfully with BOOT + KEY while visible. Fragmented/malformed command tests and batched service checks passed.
- Display interval: 3.000236 seconds in smoke test; 3.006176 seconds under repeated USB status traffic. These are software-observed intervals, not optical wake latency.
- After repeated wake, RTC time remained valid and PMIC/audio/IMU reads succeeded. Radio power control was 0x23; unused access-clock and functional-clock masks were both zero.
- Archived reference checksums passed.

Image SHA-256: `fec3356b1dce47c8a1e9570668f09e079174e2b92ec030aca14b3995be695dca`. Image is 12,272 bytes including its load-image wrapper. Linked retained allocation ends at 0x50002fe0, leaving 4 KiB reserved for stack plus 32 bytes before that reservation. The linker rejects growth into the stack reservation; this does not measure worst-case stack use.

## Important correction found during qualification

CPU-off wake resets GPIO output enables and routing despite retained TOP power. This left software I²C and the display disconnected. `board_resume()` now restores MCU pin levels/routing/enables and software I²C before activation, without resetting external devices. The ROM also re-enables UART0 clocks, so unused-clock gating is reapplied after wake. Radio force-off survives the tested transition; radio initialization runs only at cold boot.

See [investigation history](investigation.md) and `promotion-pins-wakes.log` for the failed-candidate evidence. Earlier logs in this directory describe intermediate candidates; final evidence uses the `promotion-restored-` prefix. The apparent correlation with repeating radio shutdown did not establish causation: the failure reproduced without that call.

## Operating and qualification limits

Touch is the supported CPU-off wake input. BOOT/KEY work while awake; hold both while digits are visible for USB maintenance, or hold KEY during startup. `tools/clock.py sleep` leaves maintenance. The active-work watchdog pauses during intentional CPU-off standby and cannot detect a permanently lost wake event.

Full TOP shutdown is unresolved. No battery-input current, optical wake latency, or multi-day unattended soak has been measured. This release establishes functional CPU-off standby, not minimum-power full-chip deep sleep or a demonstrated battery-life target.

## Source readability cleanup

After qualification, standby power configuration and touch-wake arming were split into small named internal functions. Remaining register literals were named, ROM calls received typed names, and wake/return contracts and board pin groups were clarified. Host tests passed again. The rebuilt clock.bin is byte-for-byte identical to the qualified image (same SHA-256 above), so no reflash or additional physical test was needed. See readability-build-test.log.
