# Repository and release maintenance

Original code uses MIT. Espressif-derived code retains its Apache-2.0 notice and license.
Vendor references are indexed in [references](references/README.md), with source URLs and archived hashes.
Local vendor snapshots, flash backups, investigation notes, raw logs, and experiments are excluded from Git.

## Release procedure

1. Update the firmware version in `src/app/app.c`.
2. Run `make` and `make test`.
3. Check tap wake, orientation, fades, repeated sleep, and USB access while the display is held on.
4. Record the result and any limits in the GitHub release notes.
5. Commit the changes.
6. Create and push a matching version tag, such as `v0.11.1`.

GitHub Actions builds from that tag on Linux. It publishes `clock.bin`, `clock.elf`,
`clock.map`, `SOURCE_COMMIT`, `TOOLCHAIN.txt`, and `SHA256SUMS` only after tests pass.
The installer downloads release assets and verifies the firmware checksum before flashing.
Checksums detect corruption; they are not an independent signature of the release publisher.

Retained RAM intentionally contains both code and mutable state in an RWX LOAD
segment. The build suppresses the linker's RWX-segment warning for this layout;
ELF segment flags do not establish a hardware memory-protection policy.

## Remaining hardware qualification

Version 0.11.0 passed physical checks for KEY hold and release, followed by USB status,
time setting, and the sleep command. Firmware flash verification and host tests passed.
Version 0.11.1 also passed a physical check that releasing an expired hold starts a smooth fade immediately.
Host tests cover both early release and expired release, including RTC refresh without timeout changes.
Host CI does not replace physical checks.
Current, battery lifetime, optical wake latency, and multi-day reliability remain unmeasured.
