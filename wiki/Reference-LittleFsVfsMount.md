

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
