# Repository and release maintenance

Original code uses MIT. Espressif-derived code retains its Apache-2.0 notice and license.
Vendor references are indexed in [references](references/README.md), with source URLs and archived hashes.
Local vendor snapshots, flash backups, raw logs, and experiments are excluded from Git.
Historical notes can refer to these local-only files. They are not public release dependencies.

## Release procedure

1. Update the firmware version in `firmware/app/main.c`.
2. Run `make` and `make test`.
3. Check tap wake, orientation, fades, repeated sleep, and USB maintenance on the board.
4. Record the result and any limits in `docs/releases/`.
5. Commit the changes.
6. Create and push a matching version tag, such as `v0.10.2`.

GitHub Actions builds from that tag on Linux. It publishes `clock.bin`, `clock.elf`,
`clock.map`, `SOURCE_COMMIT`, `TOOLCHAIN.txt`, and `SHA256SUMS` only after tests pass.
The installer downloads release assets and verifies the firmware checksum before flashing.
Checksums detect corruption; they are not an independent signature of the release publisher.

The bare-metal linker can report an RWX LOAD segment. Retained RAM contains both code
and state; this is intentional for this memory layout, not a hosted executable permission policy.

## Remaining hardware qualification

The 0.10.1 behavior passed physical checks. The 0.10.2 packaging refactor preserves event order,
but requires a new physical regression check. Host CI does not replace that check.
Current, battery lifetime, optical wake latency, and multi-day reliability remain unmeasured.
