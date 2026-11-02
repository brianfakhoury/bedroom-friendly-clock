# 0.9.0 promotion — validation pending

The root source now uses HP CPU-off standby with the complete clock in retained LP SRAM. TOP/AON, PLL/crystal and supply settings remain retained; this is not minimum-power full-chip deep sleep. The LP CPU remains inactive. There is no periodic wake or recovery timer.

The first flashed candidate passed host tests, USB smoke/service tests, and user confirmation of time and fades after touch wake. Post-wake POWER readback found UART0 access and functional clocks enabled by the ROM (both unused-clock masks were 1); the radio-domain control remained 0x23.

A second candidate reapplies radio and unused-clock shutdown during standby_resume. It built and flashed successfully, but the user reported that tapping did not show the time. USB remained enumerated but STATUS returned no response. This candidate has **not passed wake validation** and promotion is not complete. Whether this is related to the added shutdown calls, sleep entry, or another state has not been determined. Do not interpret the earlier successful smoke/service logs as verification of this second image.

The archived build/flash logs describe the second candidate; smoke/service/power logs describe the first. No battery current or optical wake latency has been measured.

The next candidate removes only the repeated radios_disable() call, keeping unused peripheral clock gating after wake. A touch then displayed the time, confirmed by the user. USB readback showed valid RTC time, responding PMIC/audio/IMU, radio control 0x23, and both unused-clock masks zero. The preceding invalid RTC result was therefore not proof of lost time: the I²C bus was failing to communicate. The exact mechanism behind the failed candidate remains unproven. The radio shutdown policy still runs during cold initialization; its force-off state survives the tested sleep/wake.

A final USB smoke attempt timed out because maintenance was not retained; repeated taps and maintenance entry have been requested before rerunning it.

Repeated-wake testing reproduced the shared-I²C failure with radios_disable removed from the wake path. Therefore the earlier apparent recovery after removing that call does **not** establish causation. The first activation after flashing can work while later activations fail. The final readback again showed all I²C peripherals failing, despite responding USB and radio/clock masks as expected. A temporary diagnostic now prints GPIO output, enable, input, I²C mux/selection and LP mux/hold state before activation. Its automatic flash connection failed; physical recovery into maintenance was requested. This diagnostic has not yet been installed.

## Pin-reset evidence and fix

The diagnostic captured cold state followed by repeated reset-reason 0x5 / GPIO-wake-cause 0x4 entries. Before sleep, GPIO output enable was 0x00d8885f, I²C GPIO7/8 IO mux registers were 0x1b00, and output selectors were 0x280. After wake, output enable was zero, both mux registers were 0xa00, and selectors were 0x80. LP mux and hold registers remained zero after touch handoff. Thus retaining TOP power did not preserve GPIO configuration across this reset/wake path. This directly explains disconnected display outputs and failed software I²C; it is not evidence that the external RTC lost its time.

The fix adds board_resume() to restore board GPIO output levels, routing, enables and pull-ups, then reinitialize the software I²C bus before reading external devices. It does not reset the panel, touch controller or RTC. Active-low chip selects and touch reset are set high before enabling their outputs. Audio clock/data pins are returned low. The wake path also gates ROM-enabled unused clocks; it does not repeat radio initialization. Temporary pin diagnostics and forced cold maintenance were removed. Repeated-wake validation is underway.
