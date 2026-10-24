

# LittleFsVfsMount (optional ESP-IDF VFS adapter)

This optional concrete mount owner binds an existing user-provisioned
LittleFS partition using upstream `joltwallet/littlefs`. It never
autoformats, returns explicit mount/unmount failures, and does not
introduce another FileStorage implementation. Compose with the
existing `LittleFsConservativeFileStorage` alias; currently
Restart-only and None interruption-atomicity. See
[docs/LITTLEFS-VFS-BINDING.MD](../docs/LITTLEFS-VFS-BINDING.MD)
for constraints, tests and outstanding strict PowerLoss qualification.

## Native hard-power evidence

Same-file ReplaceFile was **not atomic** when electrical power was cut:
the earlier ACKed file recovered as 0 bytes (RPI400
20261023T134754-502ed2c6 FAIL, independently extracted image).
A separately named **immutable** file campaign recovered complete
generations 1–5, with g0006.bin incomplete following real VDD cut
(RPI400 20261023T171847-7a7fe516 PASS).
This establishes a candidate retention architecture, not a generic
atomic replacement guarantee or production flash-endurance budget.

## Native partition I/O and bounded flash-wear evidence (2026-10-24)

The optional test-only `tests/hil/littlefs-io-profile-espidf` fixture
counts actual native LittleFS `esp_partition_write` and
`esp_partition_erase_range` calls using linker wrappers. It **reuses**
the existing RPI400 GPIO17 bounded-HIL monitor and FileStorage provider;
no second provider is implemented. Validated ESP32 test
`20261024T164137-b488e678` PASS, separately checked PowerLoss
Category A (externally verified VDD-off, reset reason SW_RESET);
both protected record generations 22 and 23 intact.
Measurement over 18 new 8192-byte generations: 1.0816x SDK write
amplification, 58 4096-byte erase calls; maximum 4 erased operations
on one block observed since boot; minimum free heap 278,568B.
These are bounded workload counts, NOT device-endurance certification
or generic file atomicity. The production FileStorage profile
remains Restart retention and None interruption atomicity.
