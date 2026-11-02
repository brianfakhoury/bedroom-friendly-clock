# Hardware

The clock uses the Waveshare ESP32-C6-AMOLED-2.16 board.
See the [reference index](references/README.md) for schematics, datasheets, and vendor source revisions.

## Devices

| Device | Function |
| --- | --- |
| ESP32-C6 | Processor, USB, and sleep control |
| PCF85063A | UTC time and calendar |
| CO5300 AMOLED | 480 × 480 display |
| CST9220 | Touch detection |
| QMI8658 | Orientation sampling |
| AXP2101 | Power supplies and battery charging |

Waveshare identifies the panel and touch controller as CO5300 and CST9220.
Its examples also use SH8601 and CST9217 compatible driver names.

## Wiring

`firmware/app/board.h` defines the board pins.
Panel chip select is GPIO15. Touch interrupt is GPIO5. Touch reset is GPIO11.
The panel uses zero column offset and 480 × 480 geometry.
Some vendor examples swap panel chip select and touch interrupt. The schematic and
XiaoZhi configuration agree with the pin assignment used here.

Touch uses I²C address 0x5A and a 16-bit register pointer.
The driver reads ten bytes from 0xD000 and checks the packet marker and touch state.
The QMI8658 uses address 0x6B. Each activation briefly enables its accelerometer,
then disables the sensors and high-speed oscillator.

## Power supplies

ALDO2 connects to the speaker amplifier CTRL input through a 10 kΩ resistor.
It does not supply touch reset. Firmware disables ALDO2 and enables output discharge.
ALDO1 remains available to the audio devices on the shared I²C bus.
ALDO3 controls panel reset.

The external RTC needs continuous power to preserve valid time.
A USB power interruption without a battery can require setting the time again.

## Battery

The board supports a single-cell, nominal 3.7 V rechargeable lithium battery.
The product page calls the connector MX1.25; the schematic calls it GH1.25.
These names do not establish a verified mating connector. Check fit and polarity before connection.
The schematic labels pin 1 as battery positive and pin 2 as ground.
Battery operation and charge-setting suitability have not been tested for this project.

AXP2101 exposes voltage and temperature measurements, but no readable load-current channel.
Charge-current settings are not measurements of consumption.
