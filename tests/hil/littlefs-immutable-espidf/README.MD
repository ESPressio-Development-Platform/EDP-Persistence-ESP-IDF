# Immutable generation file experiment — LittleFS ESP32

This fixture exists because the earlier strict FileStorage::ReplaceFile
power-loss experiment recovered the previous acknowledged `journal.bin`
as **zero bytes** after a real GPIO17 VDD power cut, despite LittleFS
mount and `seed.txt` surviving. Evidence: detached RPI400 test
`20261023T134754-502ed2c6` FAIL (correctly rejected), independent
raw postcut partition backup `20261023T135255-bb9dcbdc` PASS,
`journal.bin` 0 bytes extracted. Accordingly this test **never
overwrites a retained file in place**.

The new experiment writes generation-specific files `g0001.bin`,
`g0002.bin`, ... `g0024.bin` via existing EDP-Persistence-ESP-IDF
VfsFileStorage; it checks the target name is not already present,
writes a fixed bounded 8192-byte generation-specific payload, closes
through the established provider and independently checks full
length and content before printing ACK.

The ESP-IDF joltwallet/littlefs filesystem is explicitly provisioned
from a dedicated offline seed in a fixed private 0x90000-byte
partition at 0x170000. Production LittleFsVfsMount always has
`format_if_mount_failed=false`; only the explicit test runner
replaces this **dedicated test partition**, after first capturing a
private **complete 2 MiB** flash backup and independently verifying
flashed firmware and seeded partition.

A user-confirmed GPIO17 relay removes **actual ESP32 supply voltage**
without back-power. After the cut and ESP_RST_POWERON recovery, the
firmware scans every immutable file, validates bytes and generation,
and reports a contiguous prefix of fully valid records. A partially
written trailing candidate MAY exist and is not treated as ACKed
data. The host must check that the contiguous recovered prefix
extends at least through every generation ACKed before the relay
OFF request was initiated.

This is an intentionally limited **no-reclamation physical safety
experiment**, not the complete bounded rolling EDLS archive:
* no same-path file replaces of retained data
* maximum 24 generation files, each 8192 bytes plus LittleFS overhead
* no automatic repair/format on failure
* no validated old-file reclamation or flash endurance budget
* FileStorage still advertises conservative Restart and None atomicity

Real controller and VFS latency can prevent the power cut landing
inside a particular file write. Passing one or more physical
interruptions does not upgrade the FileStorage trait or prove
strict durability across arbitrary garbage collection. Production
Logging must add bounded retention, recovery and reclamation
without modifying acknowledged, still-retained immutable files.

Retain all raw flash backups, test UART and binaries privately on
RPI400 only. No files are uploaded to GitHub from such evidence
directories. All EDP versions remain 0.1.0.


## 2026-10-23 native immutable-file power-cut evidence

The separately named immutable-generation experiment
`tests/hil/littlefs-immutable-espidf/` built using ESP-IDF 5.5.5
and managed joltwallet/littlefs 1.22.3. Native build RPI400 detached
`20261023T171309-d038291b` PASS exit0, baseline existing provider
regression RPI400 `20261023T171752-308123d1` PASS exit0, host
GCC/Clang/UBSan contract tests remain PASS.

The true GPIO17 VDD powercut run **`20261023T171847-7a7fe516`
PASS exit0** included a **2,097,152-byte full pretest flash backup**
(SHA256 `220a1665393578975468e2fa14f4b11d958e1b0f3761eddbeaf2392160b66c11`)
and independent firmware/seed partition flash verification.
Firmware factory SHA256
`d4674ce696da3cca50d4b6a57ec2911906e7daa915b0d4f86ae44c5d643ea1d9`.
Seeded partition SHA256
`2d0657e9a97bf48c20bec2ac734f7db90438953bce812cbbf1a9c7adcc110ff1`.
Host last acknowledged generation **4** before initiating relay power-off
during `WRITE_ENTER gen5`. MCU had time to finish the complete
generation 5 and start generation 6 before electrical interruption.
After `ESP_RST_POWERON`, **generations 1–5** each had exact 8192-byte
generation-specific content and the incomplete, unacknowledged
generation 6 was **zero bytes**. All older separately named files
were intact. Host verdict TRUE_POWERON=True,
LAST_ACK_BEFORE_CUT=4, LAST_CONTIGUOUS_RECOVERED=5, PASS=True.
GPIO17 ended POWER=OFF; all raw flash, UART and image data private on
RPI400 under
`/srv/espressio/scratch/gpt6-littlefs-20261023-1318/private-littlefs-immutable-cut-20261023/`.

**Qualification boundary:** This is a single positive, end-to-end
native electrical-cut observation of independently named and closed
files, **not** proof of strict power-loss FileStorage atomicity.
Repeated varied-phase cuts, configured bounded reclamation, exhaustion,
live-mount resource ceilings, ECC/error rates and native flash-wear
budget are still required before claiming a production historical
journal. Keep conservative Restart/None FileStorage properties.
