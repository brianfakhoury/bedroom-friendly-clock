# 0.9.1 — remove USB waits from normal activation

Installed and verified: the user reports much quicker wake with correct digits/fades and rotation. Host tests, USB smoke/service tests and post-wake power-state readback pass.

Normal touch wake no longer prints wake-cause diagnostics or ON/OFF events. Those event messages remain in maintenance for host tests; requested STATUS/POWER replies are unchanged. No sleep settings, renderer or fade parameters changed. This removes possible waits for USB FIFO space on the normal activation path; no optical latency measurement has been made.

## Follow-up after user comparison

Removing USB waits did not perceptibly improve the reported delay. A maintenance-mode tap also felt the same as a sleeping tap. Repeated rendering preparation measured 0x21eaa microseconds (138,922 us) both after CPU-off wake and in a temporary cold-start maintenance build that had never slept. First cold rendering measured 235,676 us, including initial geometry clearing. This does not measure the ROM wake transition or establish total optical latency.

The candidate now caches the seven-segment masks for four digits and their ink color, writing only changed segments. The panel retains its pixels while display-off and the cache resides in retained memory. Geometry changes still erase the old layout. Framebuffer tests compare incremental results against cleared-panel rendering across all four rotations, minute values, 9/10/11/12/1-hour layouts, invalid time and ink changes; unchanged frames generate fewer than 500 pixel writes (colon only).

The wake event is also marked consumed on entry to the event loop, and on pending-touch handoff. Previously the event loop started with touch_down=false despite wake_main already activating the clock, permitting another down report from the same contact to restart rendering/fade. Earlier wake logs showed multiple ON messages for some activations, though not every additional ON was proven to be the same physical contact.

The READY banner is shorter and cold-start success messages are omitted to fit the additional renderer state/code without reducing the 4 KiB stack reservation. Initialization errors remain logged. Temporary forced-maintenance startup was removed before flashing this candidate. The final user and hardware checks passed (details below).

## Final verification and causal limits

Unchanged-frame preparation now measures 0xaf70 microseconds (44,912 us), versus 138,922 us before incremental drawing. This is about 94 ms less work, or a 68% reduction in that measured preparation interval. It includes RTC/orientation work; it excludes ROM wake and fade-to-visible time. Changed digits and orientation changes require more transfers.

The user confirmed substantially quicker response with correct digits/fades and rotation. USB smoke passed with a 3.000347-second display interval; batched service passed with 61 status replies over 3.006481 seconds. RTC remained valid, peripheral reads succeeded, radio power control remained 0x23 and both unused-clock masks remained zero. Initial final-check attempts timed out while the device was still sleeping; they passed after the maintenance gesture.

The original WFI version also redrew all digits. That work alone therefore does not explain the reported regression. The new CPU-off path introduced a separate activation before entering a fresh event loop, allowing duplicate handling of the wake contact; WFI resumes its existing loop instead. The duplicate-handling fix and incremental renderer were evaluated together, not separately, so their individual contributions to the perceived improvement are unmeasured. No optical timing or exact original-WFI-versus-new-sleep latency comparison has been established.

Final image SHA-256: `fc13fe10539d32908313e5b96892a24a455d58afb9482b57dc1e087fced1ce7f`. Linked retained allocation ends at 0x50002ff0, preserving the 4 KiB stack reservation with 16 bytes of additional margin.
