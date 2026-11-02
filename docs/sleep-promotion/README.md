# Light-sleep promotion — in progress, 2026-11-01

Saved flash release remains 0.8.3. All tests so far in this follow-up are RAM-only. Working sources are staged outside the release under `work/standby-clock` and `work/power-sleep-probe` until validation.

## PLL shutdown experiment

Changing HP_SLEEP clock power to clear XPD_BBPLL allowed a timer sleep to return (a later SNAP reported raw wake and timer cause), but USB commands stopped returning reliably. After further cycles, the user confirmed touch still displayed time. Thus USB loss was not proof of a stalled CPU. Automatic RAM loading subsequently failed, requiring a USB reseat. Toggling USB pad enable and then adding a PCR USB-controller reset did not establish restored command communication. A second reseat restored the unchanged flash clock and USB time was set again.

Espressif's `rtc_clk.c` explicitly recalibrates the PLL on restoration; the probe only requested power restoration. Missing calibration is a hypothesis to test, not yet a demonstrated root cause. Do not claim PLL shutdown is production-ready or that the returned active PLL frequency has been measured.

## Indefinite standby integration test

A new RAM-only candidate keeps the previously working PLL/crystal/supply configuration while removing the periodic wake timer. It retains a low-power GPIO5 falling-edge latch, distinguishes touch from button status, captures pending events across the pin handoff, and requests watchdog pause during HP sleep (active execution remains protected). It adds explicit USB maintenance mode and separate counters for completed and rejected sleeps. Touch responsiveness after a sleep longer than the watchdog period and maintenance entry are awaiting physical confirmation.

This is still a conservative clock-gating test, not the datasheet's sub-milliamp state and not a power measurement. No promotion to flash has occurred.

## Follow-up observation: 2026-11-01 22:30 UTC

The user reported two prompt taps and then held both buttons for maintenance. USB was present afterward, but `SLEEPSTATUS` returned `ERR COMMAND`, and `STATUS` reported an invalid RTC. This is inconsistent with the staged candidate responding in maintenance mode; do not count this as successful candidate USB recovery. The responding firmware/build and any intervening reset or power loss need to be established. Time was restored successfully with the normal USB command. Raw responses are saved in the working log `work/standby-clock/pll-maintenance.log` outside the deliverable.

## Deep-sleep alternative

Espressif documents C6 deep-sleep GPIO wake and an early HP wake stub in retained RTC fast memory. A touch interrupt on GPIO5 is a candidate wake source without running LP firmware. This is not yet a board test: interrupt capture, retained pin states, peripheral restoration, and touch-to-visible latency must be qualified. Ordinary HP RAM is not retained; our current linker places the application in HP RAM, so a stub cannot simply call the existing drivers after deep sleep. BOOT/KEY on GPIO9/10 are outside the LP GPIO0–7 set and need a separate wake strategy.

References: [C6 wake stubs](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/deep-sleep-stub.html), [C6 sleep modes](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/system/sleep_modes.html).
