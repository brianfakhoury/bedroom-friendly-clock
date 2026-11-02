# 0.10.1 — quicker fade-in, half-scale default brightness

Installed: default brightness is now 128/255, and fade-in is 180 ms instead of 250 ms. Fade-out remains 350 ms. The overall display interval remains 3000 ms, so the hold is now 2470 ms. The smoothstep curve, rendering, touch handling and TOP-off power policy are unchanged.

The USB brightness command's default also becomes 128. Explicit `--level` values and the `DIM` command still support 1–255. This is half of the panel brightness control range; emitted light and electrical power have not been measured.

The initial 120 ms / 128-brightness candidate was physically checked by the user (“looks good”) and passed USB smoke with a 3.000633-second shutoff. The user then selected 180 ms. The final 180 ms revision passed the full existing host suite, including fade endpoints/monotonicity and lifecycle checks at the updated boundary, and was flashed with hash verification. The USB smoke log applies to the 120 ms candidate, not a separate on-device timing measurement of the 180 ms revision. The ELF initialized presentation state contains brightness 128.

The focused review found no immediate blocker for this change. Outstanding hardening remains the multi-day soak, battery-input current measurements, RC transition-period calibration and voltage/temperature qualification described in [0.10.0](../0.10.0/README.md). The review is not a proof of exhaustive correctness.

Final `clock.bin` SHA-256: `57752f5b44c0c73b7a6e2a863f13d821f87edfae511977a3acb4e431bda5eecd`.
