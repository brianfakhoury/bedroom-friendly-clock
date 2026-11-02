# Power audit

Current release: [0.10.0 verification](releases/0.10.0/README.md). TOP/AON/CPU and PLL/crystal shutdown are configured and repeated timer/touch wake checks passed. LP SRAM, LP GPIO and RC_FAST remain powered for retained code and touch-edge wake; the LP CPU is inactive. Post-wake radio force control is `0x23`, and both unused-clock masks are zero. The panel remains awake but display-off. Current and the ≤100 µA battery-input target remain unmeasured. Older audits below describe their respective releases, not the current sleep policy.

## Version 0.8 baseline

Version 0.8 restores awake-but-display-off panel standby at the user’s request for faster USB-powered response. Other version 0.7 power controls remain unchanged. Panel sleep is deferred until battery operation can be measured.

No battery-input current has been measured. Register settings establish requested hardware states; they do not establish current, RF emissions, or battery life.

## Verified on the connected board

Before changes, PMU radio-domain control was `0x1c`: forced power-up and reset/isolation bypass. Radio clock-enable registers were already off; the only nonzero general modem clock field was its default data-dump mux selection. No radio stack was present. Thus the firmware was not using Wi-Fi/BLE, but had not explicitly powered down their shared domain.

Startup now asserts modem resets, disables Wi-Fi/BLE/802.15.4 clock enables and force-on controls, removes the radio-domain power-up/reset/isolation bypasses, asserts isolation/reset, then requests power-down. Readback of PMU control is `0x23` (force reset, isolation, power-down). Modem-domain registers read zero after power-down; those zeros alone are not independent proof of their internal state. The PMU control is the persistent readback evidence.

PMIC output enables changed from DC=`0x0f`, LDO0=`0xff`, LDO1=`0x01` to DC=`0x01`, LDO0=`0x05`, LDO1=`0x00`. DC1 supplies the board. ALDO1 supplies audio, ALDO2 drives the speaker amplifier CTRL input through R14, and ALDO3 drives panel reset. ALDO2 is now disabled, with PMIC 0x10 bit 5 enabling internal off-discharge to pull CTRL low (the optional R45 pull-down is NC). Earlier notes incorrectly called this a touch connection. Other adjustable outputs are not required by the schematic and are disabled. RTC backup, charger settings, and voltage values are preserved.

Audio shutdown follows the ES8311 suspend and ES7210 stop register sequences in the board vendor's audio example. Readback: ES8311 power `0xfc`, clocks `0x00`; ES7210 power `0x07`, clocks `0x7f`. ADC channels and microphone bias controls are disabled. The shared audio supply remains enabled: removing it without isolating shared I²C could back-power the devices. This is logical shutdown, not physical removal of their supply. Audio clock/data outputs are held low. Amplifier shutdown is requested through ALDO2; residual audio current remains unmeasured.

The external RTC, touch controller, USB, panel supply, and required MCU infrastructure remain available. The accelerometer already shuts down its sensors/high-speed oscillator after orientation sampling. No LP-core program is loaded. Flash and microSD supply current have not been optimized; deselecting microSD does not remove its supply.

## Unused peripheral clocks

Version 0.7 disables APB clock enables for unused UART0/1, hardware I²C, UHCI, RMT, LEDC, TWAI0/1, I²S, ETM, parallel IO, GDMA, AES, SHA, RSA, and ECC. It additionally gates timer groups 0/1, pulse counter, PWM, SDIO slave, SPI2, and digital signature access clocks, plus unused UART, I²C, RMT, LEDC, TWAI, I²S, timer/WDT, ADC, temperature, PWM, SPI2 and parallel functional clocks. GPIO, USB, SYSTIMER, flash/cache, and required infrastructure remain available. Version 0.7 is flashed; `UNUSED_CLOCKS_HEX` reads zero. Radio, audio, and regulator settings were rechecked after installation.

## Sleep experiment and recovery

A separate RAM-loaded experiment programmed an LP timer alarm and tried a conservative PMU sleep transition with CPU stall and clock gating while retaining supplies. It did not return a USB response, and automatic USB reset could not reconnect. It was not written to flash. The user recovered the device by unplugging and reconnecting USB; no battery is connected. The RTC then reported invalid time, as expected without retained backup power. USB time setting restored valid time. Version 0.6 was subsequently installed without the sleep experiment.

The PMU active/sleep tables at ROM handoff are not fully initialized for a complete sleep/wake cycle. In particular, the active clock-power and bias tables are zero despite the chip running, so copying those tables is not sufficient to describe wake restoration. This is a plausible explanation to investigate, not a proven root cause. A second RAM-only probe added active PLL/bias settings and a tested LP watchdog, but still failed to resume or recover. Both experiments required unplugging USB. The cause remains unresolved; the watchdog’s successful CPU-stall recovery does not guarantee PMU recovery. No further PMU probe is included in this release.

The ordinary clock still uses clock-gated WFI. Full-chip sleep is not enabled or validated. The ≤100 µA target is not established.

## Security boundary

The shipping application contains no networking stack, radio firmware blobs, credentials, advertising, or connection logic. Radio shutdown is applied at every application boot. It is reversible register configuration, not an eFuse lock: reflashing firmware or entering the ROM recovery path is outside this application guarantee. No irreversible security fuses were changed.

## Evidence sources

- [Espressif C6 PMU register definitions](https://github.com/espressif/esp-idf/blob/v5.4.2/components/soc/esp32c6/register/soc/pmu_reg.h), radio domain offset 0x104, force fields bits 0–5.
- [Espressif C6 modem clock/reset definitions](https://github.com/espressif/esp-idf/blob/v5.4.2/components/soc/esp32c6/include/modem/modem_syscon_reg.h).
- [Espressif C6 peripheral clock definitions](https://github.com/espressif/esp-idf/blob/v5.4.2/components/soc/esp32c6/register/soc/pcr_reg.h).
- [Waveshare schematic](https://files.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.16/ESP32-C6-Touch-AMOLED-2.16-Schematic.pdf), reviewed visually for rail connections.
- [Waveshare source](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16), commit `294543798f1a44e2f2c4d2976522323f2beee11d`, Arduino `07_Audio_Test/src/esp_codec_dev/device/es8311/es8311.c` and `es7210/es7210.c` shutdown sequences. No audio runtime/library is linked.

## POWER command

`POWER_HEX`: PMU radio control, modem clocks, force clocks, modem clocks1, force clocks1, modem reset, modem LP clocks, PMU active power.

`UNUSED_CLOCKS_HEX`: bitmask of still-enabled clock entries in `hal/power.c`; expected zero in version 0.7.

`PMIC_HEX`: registers 0x00, 0x01, 0x80, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x10. A failed I²C read prints `ffffffff`.

`AUDIO_HEX`: ES8311 0x0d, 0x01; ES7210 0x06, 0x01. Expected `fc 00 07 7f`.

The user has USB power only and no battery-current measurement setup. No battery-life or microamp claim can be validated in this configuration.

## Version 0.7 component inventory and evidence

| Component | Release state | Evidence / remaining limit |
|---|---|---|
| Wi-Fi, BLE, 802.15.4 and RF domain | Reset, isolated, forced off; no stack | PMU control `23`; no RF/current measurement |
| Unused MCU peripherals | Listed access and functional clocks gated | Both unused-clock masks zero; not a claim that every MCU block is power-isolated |
| HP CPU | WFI between events | No periodic standby tick; full PMU sleep unresolved |
| LP CPU | No program loaded or started | No claim of separately measured core power |
| AMOLED CO5300 | Version 0.8: display-off, remains awake | Faster activation; sleep-in deliberately removed. Version 0.7 sleep tests remain historical; current unmeasured |
| Touch CST9220 | Kept ready for interrupt wake | User verified repeated taps; no deep touch mode configured |
| QMI8658 | Sensors and fast oscillator disabled between reads | `IMU_HEX 41 00` |
| PCF85063 | Kept running | UTC advances; resets retain time; USB-only power loss does not |
| ES8311 / ES7210 | Software shutdown, supply retained | `AUDIO_HEX fc 00 07 7f`; shared I²C back-power concern |
| NS4150B amplifier | ALDO2 CTRL low requested through off-discharge | PMIC output `05`, common config `34`; not a voltage/current measurement |
| Unused PMIC outputs | Disabled | Output registers `01 05 00`; charger settings preserved |
| microSD | CS high, supply still connected | No independent supply switch; inserted-card current not measured |
| Flash | Supply available, no deep-power-down command | Preserve ROM/recovery behavior; flash sleep requires separate validation |
| USB and required clock/power infrastructure | Kept available | Service and ROM recovery remain usable in ordinary WFI |

`UNUSED_FUNCTION_CLOCKS_HEX` reports remaining enabled entries from the functional-clock list; expected zero. `IMU_HEX` reads QMI8658 registers 0x02 and 0x08. PMIC 0x10 readback `34` includes off-discharge bit 5. The masks cover explicit reviewed lists, not every transistor or clock on the MCU. Infrastructure such as PVT monitoring and analog control has not been indiscriminately gated.

The active-work LP watchdog is nominally eight seconds (slow RC tolerance applies). A temporary RAM-only build deliberately stalled the CPU with the release clock gates active. USB detached after 8.90 seconds; the saved 0.7 image rebooted with valid RTC time and rejected the temporary fault command. No fault command is present in release firmware. Legitimate WFI disarms the watchdog to avoid idle feed wakeups; lost wake events are therefore outside its protection.

Evidence: `hardening-readback.log`, `hardening-watchdog.log`, `hardening-idle.log`, and the host/hardware test logs. The rapid panel test's final idle observation was interrupted by loading the watchdog probe; the separate idle log supersedes that final line. No current saving is quantified.

Version 0.8 evidence is in `fast-display-build-test.log`, `fast-display-flash.log`, `fast-display-smoke.log`, and `fast-display-service.log`. Earlier hardening logs describe the sleep-enabled 0.7 build.

During version 0.8 verification, the first hardware checks stopped because the RTC reported invalid time. USB time-setting restored validity before rerunning the checks; this observation does not identify when the RTC lost its supply.

## Follow-up: sleep and watchdog investigation

A conservative RAM-only PMU sleep path passed 100 timer cycles and prompt latched-edge touch wake. The original timer bit was wrong; watchdog pause/reset scope and USB recovery were separated experimentally. The release remains WFI: USB packet wake and an indefinite-sleep recovery policy are not ready for default use. See [the full evidence and limitations](sleep-investigation/README.md). Earlier failed-probe descriptions above are historical, not the final diagnosis.


## Estimation limits: WFI versus optimized light sleep (2026-11-01)

Espressif ESP32-C6 datasheet v1.5, tables 5-10 and 5-11, gives typical CPU-idle modem-sleep current of 17 mA at 160 MHz and 14 mA at 80 MHz with peripheral clocks disabled. One light-sleep configuration is 180 microamps with CPU/radio powered down, peripheral clocks disabled and GPIOs high impedance; a more aggressive configuration is 35 microamps. These are chip-level characterization conditions, not measurements of this board or firmware. Reference: https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf .

Our release explicitly powers down the radio domain and enables selected infrastructure, so the CPU-idle table is a comparison point, not a precise WFI estimate. Our successful conservative probe retains PLL/crystal/supplies and USB clocks; neither published light-sleep figure should be assigned to it. A nearly 80–95-fold chip-level difference exists between the table's 14–17 mA idle entries and 0.18 mA light-sleep entry, but reaching that power configuration is additional work beyond promoting the probe.

Board-level savings depend on unchanged loads. Illustrative only: if MCU power changed from 50 mW to 1 mW, other loads of 1, 5, or 20 mW would produce whole-load reductions of 25.5x, 9.2x, or 3.3x respectively, before accounting for supply-conversion differences. These assumed values are not estimated measurements of the panel/touch/PMIC. Thus a 10x board-level reduction is plausible but unestablished; a precise prediction for the conservative probe is not supported. The datasheet opportunity justifies further power work, but not claiming the current implementation is near optimal or a probe promotion alone guarantees 10x.
