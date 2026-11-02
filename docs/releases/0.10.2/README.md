# 0.10.2 — repository packaging

- Separate maintenance polling and input handling from the main event loop.
- Preserve activation order, debounce, touch consumption, and the sleep handoff.
- Add a release installer with checksum verification and explicit device selection.
- Add Linux CI builds, host tests, and tagged firmware releases.
- Add the MIT license and retain Espressif's Apache-2.0 terms.
- Rewrite the README with short, direct technical instructions.
- Index hardware documents and schematic sources under `docs/references`.

Local firmware build and all five host test suites pass. Firmware size is 14,048 bytes
of text, initialized data, and BSS as reported by the compiler toolchain.
This revision has not been flashed or physically qualified. The connected board retains 0.10.1.
