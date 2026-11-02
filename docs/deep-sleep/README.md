# Deep-sleep investigation — 2026-11-01

Current outcome: **0.10.0 TOP-off is installed and functionally qualified**. The standalone source port passed 20 timer cycles; the final touch-only build passed the user's 30-second-idle, repeated-tap, rotation and fade checks, followed by USB/RTC/power checks. See [the release record](../releases/0.10.0/README.md) and [staged evidence](../../experiments/top-off/README.md). The entries below are chronological history; older “unresolved” and “currently flashed” statements describe those experiments, not the present device. Current and multi-day reliability remain unmeasured.

Status: experimental, RAM-loaded only. Saved flash remains release 0.8.3. Deep sleep has not yet been qualified for the clock.

## Proposed architecture

The ESP32-C6 has 16 KiB of LP SRAM at 0x50000000. This memory is accessible by HP and can retain code and data during deep sleep. Using it does not require running the LP CPU. The existing clock is small enough to investigate keeping the whole program here, rather than maintaining a second miniature display implementation in a wake stub. The linker must reserve and check stack headroom. Rendering speed from this memory still needs measurement.

On cold boot, initialize the board and panel normally. On a deep-sleep wake, start HP at a retained assembly entry, establish its stack and global pointer, restore MCU buses/pins, read the external RTC, and render. Do not reset the external panel or touch controller on every wake. Retain the previous digit layout and orientation so old pixels can be erased correctly. No networking, RTOS, dynamic allocation, or LP firmware is introduced.

Deep sleep is a restart path, not a return from the instruction that requested sleep. Ordinary HP SRAM and peripheral state cannot be relied upon. Pin holds must preserve panel deselection, touch reset, shared bus levels, and other board control outputs while HP powers down, and must be released only after safe output values have been restored.

GPIO5 is connected to the touch interrupt and lies within the low-power GPIO0–7 range. BOOT/KEY on GPIO9/10 cannot simply retain their light-sleep wake behavior; they can still operate while HP is awake. A touch-first maintenance gesture or a documented boot-time maintenance path is needed before promotion. The actual CST9217 interrupt pulse and wake latch must be tested on this board.

## Initial probe

A 1,204-byte program linked entirely into LP SRAM was successfully loaded and executed via the ROM downloader. A timer-triggered deep-sleep attempt then removed USB and did not restore it. This does not establish whether the stub ran. The next probe adds a visible red rectangle on wake, explicitly supplies the HP active clock masks, and preserves pad levels in sleep. Its wake entry is checked with the ROM's wake-address validation function before entry.

No current measurements are available. “Most optimal” is a hypothesis: deep sleep minimizes more MCU state, but external panel, touch, PMIC, and rail consumption remain, and board-level savings are unknown.

## References

- [ESP32-C6 wake-stub guide, v5.4.2](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32c6/api-guides/deep-sleep-stub.html)
- Espressif ESP-IDF source commit `f5c3654a1c2d2a01f7f67def7a0dc48e691f63c0`: `components/esp_hw_support/sleep_modes.c`, `components/esp_rom/esp32c6/include/esp32c6/rom/rtc.h`, `components/esp_rom/esp32c6/ld/esp32c6.rom.ld`, and C6 PMU/LP_AON register headers. Used as hardware/ROM documentation, not linked as an IDF runtime.
- ROM `esp_rom_set_rtc_wake_addr`, address `0x400000a4`, registers an aligned entry and a four-byte-aligned CRC-covered region. ROM `esp_rom_get_rtc_wake_addr`, `0x400000a8`, validates it. Mutable state and stack must not be changed inside a CRC-covered region between registration and wake.

## Visible probe and register correction

The visible-wake probe loaded and ran, but USB did not return and the user reported a dark screen. Review then identified a programming error shared by both first attempts: writing zero to PMU+0xb4 disabled LP_SLEEP_LP_REGULATOR0, rather than configuring LP digital power (which is at +0xc0). This invalidated the retained-domain assumption; the failures do not establish a wake-stub limitation. The corrected probe preserves the active LP regulator settings and now names its PMU addresses from the archived register definitions.

The next qualification is deliberately narrower: ROM-validate the registered address with `K`, then test CPU power-down while retaining TOP/AON and PLL/crystal. The LP watchdog remains armed, with sleep pausing cleared and broad chip-reset enabled, as a bounded recovery attempt. Its ability to recover this new failure mode is not yet established. This stage is not the final deep-sleep power configuration.

A link-time global-pointer relaxation overflow appeared after code size changed. The prototype now uses `-mno-relax`; successful compilation alone does not qualify wake execution.

## Registration and CPU-only test

The ROM accepted the registered entry and CRC: `STUB_ADDR 50000000`. The probe now refuses to sleep if that validation fails. The CPU-only test printed its entry message but did not return a wake-counter report. USB subsequently re-enumerated automatically and the saved clock answered `STATUS` with valid RTC time. This is consistent with the armed chip watchdog recovering the test, but the reset reason was not captured, so do not label it a proven watchdog-reset measurement. No flash update occurred.

### Reference audit boundary

PMU register offsets and bit meanings were checked against the archived C6 `pmu_reg.h`; LP timer target/snapshot/clear operations against `lp_timer_reg.h` and Espressif `lp_timer_ll.h`; watchdog chip-reset fields against `lp_wdt_reg.h`; and the stub signature, alignment, CRC validation and ROM entry addresses against Espressif `rtc.h` and `esp32c6.rom.ld`. These establish register semantics, not that our complete initialization sequence is correct.

The next required review is the complete PMU initialization performed by `pmu_init.c`, including force-power/reset/isolation overrides, regulator control handoff, clock switching, and transition timing from `pmu_sleep.c`. ROM defaults must not be treated as a fully initialized sleep configuration. Further broad power-down tests should follow that review.

## PMU override readback and reset-path isolation

A register dump after ROM RAM loading showed PMU power-control registers for TOP, HP_AON, HP_CPU, and LP_PERI each at `0x1c`: force power-up, force no-reset, and force no-isolation were set. Espressif's `pmu_power_domain_force_default()` explicitly clears these flags. The first CPU-only probe had omitted this handoff. Clearing CPU's six force-control bits produced an actual ROM report `rst:0x5 (SLEEP_WAKEUP)`, but then `boot:0x35 (DOWNLOAD...)`, rather than a retained-stub report. Thus wake reset occurred; stub execution was not established.

Clearing LP_AON_FORCE_DOWNLOAD_BOOT alone did not change that result. It is not sufficient evidence to claim this flag caused the downloader entry; retained boot strapping and the partial-domain reset are still relevant possibilities. The next test releases TOP/AON/CPU force controls together and requests their shutdown, keeps LP regulator/clock settings, and holds pads. Visible wake is awaiting user observation; USB alone is unavailable.

Raw PMU readback and serial logs remain under `work/deep-probe/`. Firmware in flash is unchanged.

## Flash-boot control experiment: first verified retained wake

The RAM-loaded CPU-only probe repeatedly reported SLEEP_WAKEUP followed by DOWNLOAD boot. A temporary command-controlled copy was therefore flashed and booted normally. With the same CPU power-down sequence, it reported SPI_FAST_FLASH_BOOT and then the retained stub reported count 1, timer cause `0x10`, and reset reason `0x5`. The user confirmed the red rectangle. Twenty further consecutive timer-wake cycles passed (counts 3–22; count 2 was a separate successful wake during test-harness adjustment). This qualifies the retained entry and CPU-only shutdown from normal flash boot. It does not qualify full chip deep sleep or touch wake.

The RAM-vs-flash boot comparison resolved the downloader-path failure for this configuration; the exact ROM branch was not disassembled. Clearing FORCE_DOWNLOAD_BOOT alone had not been sufficient.

The subsequent flash-loaded test requests CPU/TOP/AON shutdown with the corresponding force controls released, pads held, and HP sleep clocks gated according to the reference defaults. USB did not reappear within the bounded check; visible outcome is pending.

**Current flash state:** temporary command-controlled diagnostic, not release 0.8.3. It does not automatically sleep on cold boot. The known-good release image is preserved in `work/deep-probe/restore-0.8.3.bin`, SHA-256 `7cf915e01297aeeba34af9fa7c4b26392d34cbe5daadbe765f804ebf4bd1adb0`. Restore after the diagnostic session. Power cycling now starts the diagnostic, not the normal clock.

USB recovery limits: Espressif documents that disabling the USB PHY or repurposing its IO can disable automatic download. Mac-side serial reset commands require a responding USB device. A watchdog may recover CPU stalls but cannot be assumed to recover every power/USB failure. A separate reset connection or verified switchable USB-hub port would provide an independent recovery path. [Espressif download guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c6/download-guidelines.html).

## Isolated shutdown tests

The broader test was dark according to the user. A selectable diagnostic then separated the changes against the verified CPU-only baseline:

| Mode | Addition to CPU shutdown | Result |
|---|---|---|
| 0 | None | Retained wake count 1, cause 0x10, reset 0x5 |
| 1 | HP functional/APB/system clock gating | Retained wake counts 2 and 3 verified |
| 2 | LP/HP pad holds | Retained wake count 4 verified |
| 4 | HP AON domain shutdown | Retained wake count 5 verified |
| 8 | HP TOP domain shutdown | No USB response within 12 seconds; physical observation requested |

An initial serial-reader timeout in mode 1 was a harness limitation: reopening USB and querying status showed the successful retained wake. Subsequent tests explicitly reopen the connection and require the expected counter/cause/reset tuple. The roughly 1.6-second polling result includes the programmed one-second sleep, USB reconnection, and polling; it is not wake latency.

The next compiled diagnostic replaces the wake body with an assembly-only write to a NOLOAD retained marker, then returns to the ROM. It does not call C code, initialize buses, or drive the display. Normal ROM flash boot then prints the marker. This isolates entry into the stub from our restoration code. It has not yet run.

## Retained-clock integration and current state

CPU-off touch wake was confirmed with GPIO cause `0x4`, reset reason `0x5`, and two user-confirmed prompt single taps after more than 30 seconds of standby with no fallback timer. The full clock then fit in LP RAM using LTO while reserving 4 KiB stack. Existing host tests passed; USB smoke/service tests passed with roughly 3.000-second display intervals. Digits, fades, rotation and RTC access were integrated without resetting external devices on each wake.

The user initially reported touch stopping and unplugged, then clarified it was working normally and requested no recovery timer. The cause is unconfirmed; do not label it a proven flashing artifact or a resolved firmware bug. A temporary 30-second diagnostic timer had already been installed, so it was removed and touch-only indefinite standby restored. RTC time was restored after unplugging.

**Current source/flash:** `experiments/retained-clock/` prototype. This supersedes earlier notes saying that flash contains the rectangle probe or unchanged 0.8.3. The root release source is still 0.8.3. The prototype remains explicitly CPU-off with TOP and other resources retained, not minimum-power full deep sleep. No recovery timer remains. The original image backup remains available.

The minimal assembly marker-and-ROM-return test passed with CPU-only shutdown but did not restore USB with TOP shutdown. Adding reference SRAM isolation handoff, clock-switch update and sleep protection/minimum-residency setup did not resolve TOP shutdown. The exact failing stage is not established.

TRM v1.2 sections 12.4.1.2 and 12.4.2.4 identify TOP as Peripherals + ROM; its power-down control also affects HP SRAM banks unless masked. LP SRAM is separate. This broader loss of state distinguishes the failing transition from the CPU-only path. The TRM PDF is now archived in references with a checksum.

## Root release promotion attempt

The retained clock has been integrated into root firmware as the 0.9.0 candidate. This supersedes the earlier source-location note. Final wake validation is pending after a post-wake unused-clock fix; see [the release record](../releases/0.9.0/README.md). Successful prototype observations must not be treated as proof that the changed release candidate passes.

## Promotion completed

Root 0.9.0 is installed and passed repeated touch wake plus host/USB/power-state checks after the GPIO restoration fix. Retained TOP power did not preserve GPIO configuration across the reset/wake path. See [release verification](../releases/0.9.0/README.md) for current evidence and limits; it supersedes the preceding pending-validation note.

## TOP follow-up after 0.9.1

The working 0.9.1 image is saved at work/top-probe/restore-0.9.1.bin. A command-controlled diagnostic has temporarily replaced it; cold boot stays awake. Restore the release explicitly before finishing this session.

The renewed CPU-only control passed with reset reason 0x5, timer cause 0x10 and retained marker/count. TOP plus HP SRAM power-down masking (PMU_POWER_PD_MEM_MASK_REG low 15 bits set) lost USB and the user saw a dark screen. Adding the documented clock gating and LP/HP pad holds to both control and TOP cases separated another failure: the control showed the red rectangle despite losing USB, whereas the matching TOP case stayed dark. Missing USB is therefore not sufficient evidence of failed HP wake.

The next diagnostic moves a visible panel command ahead of SYSTIMER, I²C, USB, and normal board restoration. It releases pad holds through documented PMU/LP_AON registers and uses a simple instruction delay for command-only SPI. An EARLY_ONLY option stops there, ensuring the control rectangle cannot be attributed to later restoration code. The panel rectangle is loaded before sleep. This test is prepared but not yet installed.

References reviewed: TRM v1.2 §12.4.2.4 and register 12.37 (HP SRAM masks), pmu_init.c force-control handoff and active/sleep state setup, pmu_sleep.c timing setup, pmu_param.c/.h defaults, PMU immediate pad-hold bits and rtc_io_ll.h. Additional reference files are now archived at the pinned ESP-IDF commit with checksums. Retained supplies and PLL/crystal are intentional here; this is isolation of TOP behavior, not a complete minimum-power configuration.

The early-only CPU control (mode 0x20) passed: ROM reported SLEEP_WAKEUP, and the user confirmed the rectangle while execution stopped before normal restoration. USB automatic reset could still reflash this control without unplugging. The early-only TOP case (mode 0x3b: TOP, SRAM masks, pad holds, clock gates, early-only) has been triggered; USB detached and the physical result is pending. A dark result would still not prove the stub never executes: GPIO access itself depends on restored TOP state.

The early-only TOP case stayed dark, confirmed by the user. Preparing an independent LP recorder exposed a documented memory-mode requirement before any LP firmware was introduced: TRM v1.2 p172 says LP SRAM must use low-speed access when HP sleeps; Espressif lp_core_ll_fast_lp_mem_enable(false) also selects this mode for deep-sleep LP access. The existing probe left high-speed access selected (LP_AON_LPBUS = 0xb0200000).

A CPU-only control switching LP_AON_FAST_MEM_MUX_SEL to zero and issuing UPDATE passed timer wake and retained-stub entry; post-wake readback was 0x10200000. The same low-speed selection has now been applied to the early-only TOP test. Physical result pending. LP CPU remains inactive; no LP recorder firmware has been loaded.


## LP recorder and JTAG follow-up

The low-speed-memory TOP test remained dark. A temporary LP-core recorder was added to the isolated diagnostic, not to the clock. Espressif OpenOCD v0.12.0-esp32-20250422 now provides HP/LP register and memory inspection over built-in USB JTAG.

JTAG identified an LP instruction-fetch permission fault: LP_TEE_M0_MODE_CTRL was 3 (REE2), while LP_APM region 0 covered the address space with no permissions. The debugger could read valid instructions even though LP execution faulted at its reset entry. Setting the diagnostic LP master to trusted mode (0) allowed the recorder to execute. This is diagnostic-only access; command Z stops LP and restores access/clock/debug defaults before returning to production firmware.

The awake control recorded PMU state and used a five-second LP deadline to reset HP through LP_AON_HPSYS_SW_RESET. USB returned and the retained recovery marker was readable. This proves that control path with HP awake, not during TOP shutdown.

The subsequent TOP test lost USB, including the JTAG interface, and did not recover within the observation window. No retained trace was retrieved. Do not infer that the LP recorder survived the transition, or that HP failed before entering its stub. The next control must qualify the recorder/recovery across CPU-only sleep with the same clock gating and pad holds before relying on it to interpret TOP failure.

Raw diagnostic source and logs are currently in work/top-recorder; the working 0.9.1 image remains preserved separately. Flash still contains the temporary diagnostic. No new production sleep mode has been qualified and no current has been measured.


### Recorder sleep control and recovery limits

The CPU-only control with low-speed LP memory, clock gates and pad holds (mode 0x53) produced six retained PMU samples and recovery marker 0x80000000. JTAG retrieved the trace after USB serial stayed silent. HP was executing the cold-boot diagnostic and later its command loop; a silent serial port was not a stalled processor. Automatic esptool reset/reflash also succeeded without unplugging. See work/top-recorder/cpu-trace-jtag.log and cpu-progress.log.

The matching TOP case (mode 0x5b) printed its confirmed entry, then removed the whole USB device; neither serial nor JTAG was available afterward. The initial LP recovery forces domains up and requests HP reset, but does not explicitly restore gated clocks and held pads. An expanded recovery sequence is compiled for qualification; it is not a production wake solution and has not passed yet.

The register audit also corrected a misleading production symbol: 0x600b2800 is LPPERI_CLK_EN_REG, not LP_CLKRST_LP_CLK_CONF_REG. Bit 27 still enables the LP IO clock. The operation and address are unchanged. LP startup/access headers are archived at the existing pinned Espressif commit in references/manifest.json.


### Recovery-control qualification and marker correction

Explicit clock/pad restoration before LP-requested HP reset restored both JTAG and serial in the CPU-only control. The matching TOP trial still removed USB. No TOP trace was retrieved. These remain diagnostic recovery tests, not a qualified production wake path.

An observation defect was found before interpreting the missing wake marker: cold main cleared wake_trace on software reset, and the recorder only sampled it when PMU state changed. Thus a zero marker after recovery does not prove the retained entry failed. The next diagnostic clears markers at trial start, preserves them through recovery, and records a sample when the marker changes even if PMU state is unchanged. CPU-only wake must be rechecked with that instrumentation before drawing further conclusions about TOP.

The production LPPERI register-name correction was rebuilt; clock.bin remains byte-identical to preserved 0.9.1 (SHA-256 fc13fe10539d32908313e5b96892a24a455d58afb9482b57dc1e087fced1ce7f). Production behavior is unchanged.


The corrected CPU-only trial captured a seventh sample containing wake marker 0xd33f0001 and retained wake count 1 after recovery (work/top-recorder/marker-cpu-jtag.log). This confirms retained entry runs with the LP recorder active. Earlier absent markers were insufficient evidence of a wake failure.

Following that recovery, esptool connected but its uploaded RAM flashing stub raised an illegal-instruction exception. ROM-only flashing with --no-stub succeeded without unplugging. The cause of the RAM-stub exception is not established; do not confuse it with failure of the ROM download connection. A diagnostic variant now separates the LP recovery timer from PMU-state sampling to test whether those debug-register reads block during TOP transition.


A timer-only LP recovery control (command U, no PMU-state reads while armed) recovered HP awake and returned marker 0x80000000. Its TOP trial (W) again removed USB. The host did not capture the final entry message for that trial, so the failure cannot yet be assigned to the actual sleep transition. The next diagnostic adds paced serial checkpoints around memory masking, ROM registration, PMU state tables, TOP force-control release, and final sleep entry. It will be run from a fresh boot to avoid conflating state left by a prior recovery with the TOP transition.


The paced fresh-boot TOP trial reached every setup checkpoint, validated the retained entry, and printed ENTER DEEP TIMER before USB disappeared (checkpoint-top.log). Keeping HP memory unisolated throughout the otherwise matching trial also failed to restore USB (memory-isolation-top.log, mode 0xdb). SRAM isolation alone is not a demonstrated fix.

The next isolated variable is the LP fast-clock source. TRM v1.2 register 8.75 documents LP_CLKRST_FAST_CLK_SEL=1 as XTAL_D2 and 0 as RC_FAST; both LP active/sleep oscillator settings currently retain RC_FAST. Diagnostic command B selects RC_FAST by clearing bit 2 at 0x600b0400. This tests clock independence; it is not yet a proven requirement for TOP sleep. Production clock selection is unchanged.


## Working Espressif TOP-off baseline

The minimal ESP-IDF v5.4.2 diagnostic passed five consecutive retained timer deep-sleep wakes with USB returning each time. A separate breakpoint capture at pmu_sleep_start confirms TOP shutdown enabled (HP_SLEEP_DIG_POWER=0xe8200000). This is a working same-board reference, not a fix to the bare-metal firmware. Source, tested configuration, raw logs, register snapshots and limitations are archived in [experiments/idf-baseline](../../experiments/idf-baseline/README.md). Production remains strict bare-metal.


After the reference-baseline session, bare-metal release 0.9.1 was restored and flash verification passed. RTC time was set and USB status/display/power checks passed. This supersedes earlier temporary-diagnostic flash-state notes. TOP-off remains experimental; the restored clock uses its qualified CPU-off/TOP-retained standby.


The completed [source/register comparison](../../experiments/idf-baseline/comparison.md) identifies omitted analog regulator handoff and incomplete state/timing initialization, plus clock/domain-policy differences. It also corrects the interpretation of the LP memory-mode and CPU reset-vector registers. These are ranked differences, not a proven root cause. The production image was not changed during this audit.


## Retained-entry isolation and standalone hardware initialization

The IDF-backed custom C wake marker passed five matching timer/stub counts. A fresh-stack assembly variant initially failed because its object was not pulled from the component archive: nm showed only the weak default handler. Explicit registration of stack_wake_entry corrected the test build, after which five timer/stub counts matched and the recorded stack pointer was inside its dedicated retained stack. Sources/logs are under experiments/idf-marker and experiments/idf-stack-entry. This qualifies those marker paths, not the clock application.

A standalone probe in work/bare-top links the reference PMU/calibration/analog-I2C hardware objects with our startup, linker, USB and timer code. No RTOS runtime is linked; nm -u reports no unresolved symbols. This is temporary port scaffolding, not the intended final dependency layout. Its initial sleep used TOP/AON/CPU/radio/VDD_SPI shutdown (0xe8200000). JTAG verified wake count one, sleep reset reason and execution in the command loop, despite silent serial. A second wake triggered through a JTAG mailbox advanced the count to two.

The serial failure was independently identified: USB_SERIAL_JTAG EP1 FIFO config read zero (no TX space), and writing WR_DONE=1 through JTAG immediately recovered serial with wake count two. Our usb_puts waited for space before committing a full packet; a timeout could return with an uncommitted packet. The helper now commits when the FIFO is full and on timeout. This accounts for hidden successful wakes in this standalone probe; it does not retrospectively explain every earlier disappearance of the entire USB device. Repeated serial-observed wake qualification is underway.


## Standalone TOP-off qualification and full-clock integration status

The standalone hardware-initialization probe passed five serial-observed TOP-off timer wakes both when returning through ROM and when resuming directly on a fresh retained stack. Logs: work/bare-top/return-cycles.log and work/bare-top/cycles.log. This establishes feasibility without an RTOS, but uses selected Espressif hardware initialization objects as temporary scaffolding; it is not yet an independent, reduced register-level port.

The full-clock qualification image in work/clock-top was flashed and its RTC set. Its first timer-sleep trial produced no captured wake output (timer-wakes.log). Built-in JTAG could not identify the target afterward, including on a subsequent connection attempt. These observations do not establish where execution failed. Display/touch wake and repeated TOP-off cycles are not qualified in this integrated image. The board currently contains this experimental image; the preserved 0.9.1 image remains work/top-probe/restore-0.9.1.bin. No standby-current measurement has been made.

Architectural decision remains open: ESP-IDF has a working same-board sleep baseline and would remove ownership of the platform power sequencing from the application, at the cost of the original no-RTOS/no-IDF-runtime constraint. Bare-metal feasibility is now demonstrated, but integration and long-run reliability remain unfinished. A runtime migration has not been authorized or performed.
