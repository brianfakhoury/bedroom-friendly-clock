# Wi-Fi under the bare-metal constraint (historical investigation)

**Decision: Wi-Fi was removed from scope on November 1, 2026.** The RTC and occasional USB corrections meet the intended use. The options below record the earlier investigation; they are not an implementation plan.

The current decision is to keep the clock strict bare-metal C/assembly and explore Wi-Fi separately. No network stack or vendor radio binary is linked into version 0.3. USB time setting remains available, and the external RTC continues without network connectivity.

## Evidence

Espressif's [ESP32-C6 esp-radio 0.18.0 documentation](https://docs.espressif.com/projects/rust/esp-radio/0.18.0/esp32c6/esp_radio/index.html) demonstrates that Wi-Fi need not use ESP-IDF itself. However, that supported alternative requires a preemptive scheduler and dynamic allocator, and uses precompiled radio driver blobs. Disabling its default allocator requires supplying allocation functions; it does not remove allocation. Its scheduler exception applies to 802.15.4, not Wi-Fi. It also uses Rust rather than this project's C/assembly. Therefore it is not a drop-in solution to the established constraints.

Espressif's [ESP-AT TCP/IP command documentation](https://docs.espressif.com/projects/esp-at/en/latest/esp32c6/AT_Command_Set/TCP-IP_AT_Commands.html) provides SNTP configuration, time query, and synchronization interval commands. These could be accessed from a bare-metal host through a separate communications module; that module would carry its own vendor firmware dependencies.

## Options and recommendation

| Approach | Constraint impact | Assessment |
|---|---|---|
| Keep USB time setting | Preserves all firmware constraints | Working baseline; no autonomous network correction |
| Port onboard Wi-Fi blobs to a custom adapter | Requires investigation of scheduling, allocation, synchronization, and radio dependencies | Substantial research, not a demonstrated no-RTOS/no-allocation solution; renaming a scheduler would not satisfy the intent |
| Separate bootable synchronization image on the same C6 | Clock application stays bare-metal, but the chip runs an RTOS during synchronization | Potentially practical without new hardware if the user later relaxes the runtime constraint; boot layout, recovery, and interruption behavior need design |
| External UART Wi-Fi module running ESP-AT | Main C6 firmware stays bare-metal; adds hardware and vendor firmware on another chip | Clearest separation if the constraint applies strictly to the clock MCU; available pins, power switching, and module compatibility must be verified first |

Keep the working offline build for now. If no additional hardware is acceptable, investigate an isolated synchronization image only after explicitly revising the runtime constraint. If strict bare-metal operation on the clock MCU remains absolute, investigate a separately powered communications module. Neither architecture has been implemented or electrically validated here.

Any future sync path should obtain UTC, validate the returned date, update the PCF85063 only on success, and impose bounded connection/retry time. Failed synchronization must leave the existing RTC time intact and return to standby. The Wi-Fi power budget must include association and retries, not just idle current.
