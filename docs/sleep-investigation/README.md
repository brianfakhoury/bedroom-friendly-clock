# Bare-metal sleep and watchdog investigation — 2026-11-01

## Outcome

A conservative ESP32-C6 PMU sleep/wake path works without an RTOS or LP-core firmware. It stalls the HP CPU and gates its system clock and most peripheral clocks, while retaining SRAM, regulators, PLL, crystal, and USB clocks. This is a functional sleep demonstration, not a minimum-current configuration. No current measurement was available.

The normal clock remains version 0.8 in flash, using WFI. All sleep experiments were RAM-loaded. The final forced missing-wake test reset back to that saved clock, retained valid external RTC time, and restored USB. Normal hardware checks passed afterward (see restored logs).

## Findings with evidence

### 1. The original timer wake bit was wrong

The original probe selected PMU bit 3, which is Wi-Fi beacon wake. The C6 LP timer uses bit 4. This was an implementation mistake, not a hardware limitation. Espressif's `pmu_bit_defs.h` establishes the mapping. The LP timer independently counted and raised its alarm before any sleep test.

After correcting the wake source, both CPU-stall-only and HP-system-clock-gated transitions returned with wake cause `0x10`. With USB clocks retained, **100 consecutive timer sleep/wake cycles passed** (`cycles-usb-retained.log`, `CYCLES 00000064`).

### 2. Watchdog recovery has two separate requirements

Controlled comparisons of the original failing probe:

| Watchdog pause during HP sleep | Reset scope | Observation |
|---|---|---|
| Requested | Ordinary system reset | Original failure reproduced; USB unresponsive and user reported no display activation |
| Requested | Chip reset including analog circuitry | USB recovery still failed |
| Cleared | Chip reset including analog circuitry | Saved clock and USB returned, RTC valid |
| Cleared | Ordinary system reset | USB disconnected and did not return, but **user confirmed the clock displayed time** |

Thus a missing USB connection was not sufficient evidence that the CPU failed to recover. The ordinary reset restarted the application in that test, but did not restore usable USB. Chip-reset enable (`LP_WDT_CONFIG0` bit 8, reset width 20 slow-clock cycles) recovered both under the tested conditions. The experiment keeps PMU `HP_SLEEP_DIG_PAUSE_WDT` clear and also keeps the watchdog's own sleep-pause bit clear.

A previous diagnostic left the CPU executing while the PMU sleep state was requested. Its watchdog behavior did not predict the CPU-stalled case. That was not an equivalent experiment and is not evidence against the controlled comparison above.

The exact internal USB controller/PHY state was not captured, so this is not attribution to a particular silicon erratum. The broader reset is a tested recovery choice, not proof that it will recover every possible electrical/power fault.

Final test: deliberately omit all wake sources, leave the watchdog armed, observe USB detach, then query the saved flash release successfully with valid UTC. See `final-fault.log` and `final-recovery.log`.

### 3. Touch needs an event that survives sleep

Level-triggered touch experiments initially appeared to wake, but elapsed LP-counter measurements showed they waited for the fallback timer. Wake cause `0x14` combined a pending GPIO condition with timer wake; it did not prove prompt touch wake. The user correctly reported roughly a second of delay or missed taps.

The working experiment routes GPIO5 through the low-power input hardware and enables **latched falling-edge wake**, with the LP IO clock enabled and the edge latch cleared around each wait. It restores the normal pin configuration after waking. It also preserves a touch-wake event for the application instead of requiring the finger still to be down when the touch packet is read. Diagnostic output is deferred until after input handling.

The user confirmed prompt single-tap behavior. `idle-edge-followup.log` records four activations with GPIO-only wake cause `0x04`, GPIO/LP pending bit `0x20`, and elapsed counts `0x24a62`, `0x1347d`, `0x1344c`, and `0x26220`, all below the fallback target `400000` (`0x61a80`). These are time spent asleep until the tap, **not optical wake-latency measurements**. Button wake had been observed by the user in an earlier probe; it has not been characterized as thoroughly as final touch wake.

The low-power GPIO hardware operates without a program on the LP CPU. This is a useful distinction between using the LP subsystem and using the LP core.

## Why this is not the default release yet

The probe uses a roughly three-second LP-timer fallback and an active watchdog to make experiments recoverable. Ordinary USB serial commands did not reliably wake this mode immediately; they could wait until that fallback. Keeping USB clocks enabled preserved communication after wake but did not establish packet-triggered immediate wake.

Shipping that fallback as a power optimization would introduce periodic standby wakeups and USB latency without a measured energy benefit. For the current USB-powered clock, the responsive WFI release remains the default. For battery use, the next design decision is when to enter PMU sleep, which interfaces must remain responsive, and how to protect legitimate indefinite sleep without periodic watchdog feeding. More aggressive PLL/regulator/power-domain changes also remain untested.

## Simulation and test strategy

Local host tests cover calendar, geometry, animation, and application lifecycle. Available C6 emulators can help with supported CPU/peripheral behavior, but this investigation did not validate any emulator's PMU, GPIO pulse, or USB reset fidelity. No emulator was installed or used to establish the results above. The decisive evidence came from controlled register changes, LP-counter timing, actual reset/recovery tests, and physical observation. Emulation should be adopted for a specific supported test, not treated as proof of analog sleep behavior.

## Reproduce

The [RAM-only probe](../../experiments/sleep-probe/README.md) reuses the release drivers and replaces the event-wait/service/main files for diagnostics. The original failed variants remain in the workspace under `work/sleep-investigation/`; the release does not compile any probe code.

Earlier logs preserve unsuccessful hypotheses. In particular, `rtc-gpio-wake.log` is **invalid as a GPIO wake test**: GPIO18 was mistakenly assumed to carry RTC_INT. Schematic inspection showed it is connected to the power-button sensing circuit. The test only configured it as an input; it did not drive the pin. RTC timer/control registers were restored after that test. Do not infer wake performance from it.

Some short interaction recordings ended before the user finished tapping; the longer `idle-edge-followup.log` is the final touch evidence. Earlier overlapping serial readers are not timing evidence. The LP counter and user observations establish the final result.

## Primary references

- [C6 PMU wake-bit definitions](https://github.com/espressif/esp-idf/blob/v5.4.2/components/esp_hw_support/port/esp32c6/private_include/pmu_bit_defs.h)
- [C6 LP watchdog controls](https://github.com/espressif/esp-idf/blob/v5.4.2/components/hal/esp32c6/include/hal/lpwdt_ll.h)
- [C6 low-power GPIO controls](https://github.com/espressif/esp-idf/blob/v5.4.2/components/hal/esp32c6/include/hal/rtc_io_ll.h)
- [C6 GPIO driver wake setup](https://github.com/espressif/esp-idf/blob/v5.4.2/components/esp_driver_gpio/src/gpio.c)
- [USB Serial/JTAG sleep limitations](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32c6/api-guides/usb-serial-jtag-console.html)

Vendor sources were used as hardware references; no IDF runtime or RTOS is linked.
