# LittleFS ESP32 native partition I/O instrumentation (experimental)

This test-only fixture is a copy of the established bounded, immutable-file LittleFS HIL. It does **not** add or replace any ESPressio Persistence provider.

The only substantive difference is that the ESP-IDF test executable uses the linker's `--wrap` mechanism to intercept the upstream `joltwallet/littlefs` component's calls to `esp_partition_write` and `esp_partition_erase_range`. The wrappers forward each operation to the existing SDK function and accumulate counters only for the dedicated `edplfs` 589,824-byte partition. They do not insert failure, skip, modify, reorder or retransmit any flash operation. Each 4,096-byte erase increments a per-block counter for 144 nominal blocks.

At selected generation boundaries, `EDP_WEAR` markers report total partition API writes, programmed bytes, erase calls, distinct erased blocks, the maximum observed erase count of an individual block, native errors, current free heap and minimum free heap. To qualify these as measurements, the executable must link the wrappers successfully **and** report nonzero expected counters in its device UART log. Counters reset on each MCU boot. These measurements are logical SDK partition operations **during the observed fixture run**—not flash vendor physical erase telemetry or an accelerated device endurance guarantee. LittleFS metadata and filesystem GC cause write amplification that can be estimated from bytes passed to the partition API versus logical record bytes.

The fixture uses the established EDP-Persistence `VfsFileStorage`, optional no-autoformat `LittleFsVfsMount`, and user-approved RPI400 GPIO17 true-power interruption with the existing independently tested two-category classifier. The upstream vendor component is not modified. Every on-device run must first privately back up the entire 2 MiB ESP32 flash and independently verify the application and explicit test-only seeded partition. All raw flash/serial data are private RPI400 evidence, never version-controlled.

**Limitations:** one bounded 8 KiB file generation per commit, with two retained generations and a temporary third; finite 64-name campaign; no production EDLS content, no long-term erase-endurance validation, and no proof of full-capacity garbage-collection safety. A passing run does not upgrade FileStorage's conservative Restart/None interruption atomicity or justify a release claim. Versions remain 0.1.0, Mesh untouched.

## Actual validated instrumented outcome — 2026-10-24

Native ESP-IDF build \`20261024T163552-98f08f2f\` PASS, real
GPIO17 user-qualified hard-cut \`20261024T164137-b488e678\`
PASS exit0, independent immutable recovery g22/g23 after host ACK23,
12.278 seconds OFF, SW_RESET device reason (not POR).
\`verify_io_metrics.py --self-test\` PASS, and
\`verify_io_metrics.py <private evidence>/monitor/serial.txt\`
**AUDIT PASS**:
gen2→gen20 147456 logical record bytes, 159488 partition
write bytes (1.0816x), 58 erase calls, 62 distinct blocks
erased since boot, maximum 4 erases on one block, zero native
write/erase errors, minimum free heap 278568 bytes. The
native binary's wrappers were ELF-link verified.

Use the **canonical** \`tests/hil/littlefs-bounded-espidf/
relay_cut_monitor.py\` and \`powercut_qualification.py\`.
Do not copy/fork the classifier in this fixture.
