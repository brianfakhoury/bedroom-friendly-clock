# Hardware evidence and implementation notes

## Board identity

The handoff called the device ESP32-C6-Touch-AMOLED-2.06. USB probing established only ESP32-C6 revision 0.2 and 16 MiB flash. The saved original firmware contains `C6_AMOLED_2_16`, the board's 2.16 repository URL, and `esp32-c6-touch-amoled-2.16` source paths. The user then confirmed the physical label `ESP32-C6-AMOLED-2.16`. Visible time digits were confirmed after changing to that pinout.

## Authoritative references

- [Waveshare 2.16 schematic](https://files.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.16/ESP32-C6-Touch-AMOLED-2.16-Schematic.pdf)
- [Waveshare 2.16 source repository](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16), inspected commit `294543798f1a44e2f2c4d2976522323f2beee11d`.
- Working pinout reference: `02_Example/XiaoZhi-v2.2.5/main/boards/waveshare/esp32-c6-touch-amoled-2.16/config.h` in that repository. Panel CS=15, touch INT=5, touch reset=11; panel reset is PMIC controlled.
- Panel init and ALDO3 reset sequencing: `02_Example/ESP-IDF-v5.5.3/09_LVGL_V9_Test/components/port_bsp/display_bsp.cpp`.
- **Vendor inconsistency:** that ESP-IDF demo's `main/user_config.h` swaps CS and touch INT (CS=5, INT=15). Follow the schematic and XiaoZhi configuration instead; the connected device works with CS=15.
- [Espressif ESP32-C6 register definitions](https://github.com/espressif/esp-idf/tree/v5.4.2/components/soc/esp32c6/register/soc), inspected commit `f5c3654a1c2d2a01f7f67def7a0dc48e691f63c0`.
- [NXP PCF85063A datasheet](https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf): seconds OS bit, STOP bit, BCD calendar, and coherent sequential reads.

The Waveshare panel examples use the SH8601-compatible QSPI driver with a board-specific initialization sequence. This project uses that sequence directly and does not claim to have identified the controller through register readback (reads returned 0xFFFFFFFF). Unlike the 2.06 example, the panel uses a zero column offset and 480×480 geometry.

## Standby and timing

The external RTC is authoritative. The 16 MHz SYSTIMER (40 MHz crystal divided by 2.5) only measures rendering and the three-second active interval. Its comparator is disabled during indefinite standby. GPIO and USB interrupts route to PLIC input 1; machine global interrupts remain masked. `WFI` wakes on the pending enabled source, and the application consumes the source synchronously. This avoids an RTOS, interrupt context switching, and standby timer ticks.

`PCR_CPU_WAIT_MODE_FORCE_ON` is cleared so the CPU wait clock can gate. This is a limited idle state, not full system light/deep sleep. The display-off command hides retained display RAM. No refresh loop runs on the CPU. Touch stays awake to detect taps. The QMI8658 accelerometer is enabled briefly for orientation reads, then its sensors and high-speed oscillator are disabled. Version 0.7 shuts down the amplifier, codec, and audio ADC functions; board current still needs measurement.

The ROM's TIMG0 watchdog is explicitly disabled at startup. It was enabled during flash boot, unlike the initial RAM-loading path, and otherwise reset the board during display initialization. The release disables inherited TIMG/LP watchdog settings, then arms the LP watchdog during startup and active work. It disarms that watchdog around legitimate WFI. Lost wake events remain outside its protection. See the sleep investigation for the separately tested broader reset policy.

## Recovery

Holding BOOT while resetting/powering the chip enters its ROM download mode if ordinary automatic download entry stops working. Release BOOT after entry, then use `make flash`. Do not hold BOOT during normal reset when testing the application.

To restore the saved original image, if ever needed:

```sh
../../work/venv/bin/python -m esptool --chip esp32c6 --port /dev/cu.usbmodem1101 write_flash 0 backups/original-flash.bin
```

This overwrites the full flash with the saved image. It is not part of normal clock updates.

## Touch and fades (version 0.3)

The CST9220 uses the vendor’s CST9217-compatible protocol: I²C address 0x5A and a 16-bit register pointer. Reset is held low for 200 ms, then released for 200 ms. A falling edge on GPIO5 triggers a ten-byte read from 0xD000; the packet marker, point count, and touch state determine a new press. This follows the board repository's Arduino `lcd_touch.cpp` protocol. The user verified two separate taps with dark standby between them.

The brightness command (0x51) follows a fixed-point cubic smoothstep: 250 ms fade-in, 2400 ms hold, 350 ms fade-out. Peak brightness is 255 as requested. The active interval begins after pixel transfer; rendering latency is additional. A new activation begins at the current brightness. The panel is explicitly switched off at the end, so standby does not depend on brightness zero alone. The timing and level curve are tested; perceived smoothness still requires visual assessment on the panel.

## Orientation (version 0.4)

The board schematic identifies a QMI8658 at I²C 0x6B. The board's `02_I2C_QMI8658` example establishes WHO_AM_I=0x05, register locations, and acceleration scaling. The [QST datasheet](https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf) describes the sensor-enable and oscillator-disable controls. No vendor sensor library is linked.

Each activation enables only the accelerometer at ±2 g and 500 Hz, allows 20 ms settling, and averages four data-ready samples separated by 3 ms. It then clears CTRL7 and sets CTRL1.SensorDisable. Reads and writes are bounded; failure retains the previous display orientation. This adds roughly 35–45 ms to the activation path; no periodic standby sampling is introduced. No sensor-current measurement has been made.

Classification requires 0.73–1.28 g total magnitude, a dominant in-plane component above 0.55 g, and a margin of roughly 0.15 g over both other axes. Ambiguous readings retain the last orientation. The display rotates only on activation. The four uniform-color rectangle transforms rotate glyphs without a framebuffer or assumptions about the panel's rotation-register behavior. On a change, old segments are erased while hidden before the new digits fade in; this adds one glyph-transfer pass.

The physical upright calibration (USB edge down) measured approximately X=-875, Y=16600, Z=-150 counts, establishing positive Y as the default upright axis. A physical quarter-turn clockwise measured X=15768, Y=712, Z=-956 counts, establishing positive X as requiring a 270-degree clockwise glyph rotation (equivalently 90 degrees counterclockwise).

On-device version 0.4 checks: the clockwise calibration position selected 270 degrees; activation work measured 118165 µs without a rotation change and 196896 µs with cleanup. The USB-observed active interval remained 3.007 seconds. Host tests cover all four axis choices, flat/diagonal/moving fallback, rectangle bounds, and inverse transforms.

## Identification and rail corrections (version 0.7)

The current [Waveshare documentation](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16) identifies the panel as CO5300 and touch as CST9220. SH8601/CST9217 are compatible driver names in vendor examples, not chip-identification readbacks. CO5300 sleep-in stops panel converters/oscillator/scanning; sleep entry and sleep-out each require 120 ms before the opposite transition/normal use. Extended settings are restored after wake.

ALDO2 connects through R14 (10 kΩ) to the NS4150B speaker-amplifier CTRL input, **not touch reset**. The schematic’s optional R45 pull-down is NC. We disable ALDO2 and enable AXP2101 internal output discharge (0x10 bit 5) to pull the disabled output low. ALDO1 remains available for shared-I²C audio devices; ALDO3 controls panel reset. Earlier ALDO2 descriptions were incorrect.


## Battery connector and PMIC telemetry review (2026-11-01)

The product documentation calls the battery header MX1.25; the schematic labels J1 `GH1.25连接器2P 米白色立贴` (2-pin, natural-color, vertical SMT GH1.25 connector). Pin 1 is VBAT1 positive, pin 2 is ground. The board photo shows polarity markings. This conflict prevents claiming a verified PicoBlade/MX or JST GH mating plug solely from the published names. The earlier generic MakerFocus “JST1.25” battery recommendation is unverified and withdrawn as a guaranteed fit. Do not treat connector pitch alone as compatibility. A single-cell nominal 3.7 V rechargeable lithium battery is specified; no battery, connector fit, charge setting suitability or retention duration has been tested on this unit.

AXP2101 exposes voltage/temperature ADC channels (VBAT, VBUS, VSYS, TS, die temperature), not a readable load-current channel. Charge-current and input-current-limit settings are not measured consumption. Mac USB allocation information is also not a current measurement.

Archived source inputs are indexed in [references](../references/README.md). Product documentation: https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16 . JST GH family reference: https://www.jst-mfg.com/product/index.php?lang=2&series=105 .
