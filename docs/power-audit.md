# Power configuration

Normal standby powers down the ESP32-C6 HP CPU, TOP, and AON domains.
The main crystal and PLL are off. LP SRAM, LP GPIO, and RC_FAST remain available for touch wake.
The LP CPU does not execute code. There is no periodic timer wake.

| Component | Standby configuration |
| --- | --- |
| Radio domain | Forced off and isolated; no radio stack is linked |
| Unused MCU peripheral clocks | Gated |
| AMOLED | Powered, display off, pixel memory retained |
| CST9220 touch | Powered for tap detection |
| PCF85063A RTC | Powered for timekeeping |
| QMI8658 | Sensors and high-speed oscillator disabled |
| Audio codec and microphone ADC | Software shutdown; shared supply retained |
| Speaker amplifier | Shutdown requested through CTRL |
| SD card | Deselected; an inserted card still receives power |
| PMIC indicator | Disabled |

Register readback verifies configuration, not electrical consumption.
No battery-input current, battery lifetime, or optical wake latency has been measured.

## Remaining measurement candidates

- Panel sleep could reduce standby consumption but adds wake delay.
- Touch low-power scanning needs confirmation of reliable tap interrupts and firmware support.
- Removing audio power requires confirmation that unpowered devices do not load the shared I²C bus.

The touch driver does not explicitly request a low-power scan mode.
Automatic idle behavior is not established. A documented controller sleep command is insufficient
because tap detection in that mode has not been confirmed.

The LP watchdog protects active work. It does not detect a permanently lost wake input.
Long-duration and temperature-range testing remain outstanding.
