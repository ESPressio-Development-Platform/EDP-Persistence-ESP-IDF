# LittleFS bounded create–commit–reclaim experiment (two retained)

This is a test-only third phase, after the negative same-path ReplaceFile
power-cut and the positive no-reclamation immutable-generation hard cut.

**No existing still-retained filename is opened for writing**. The
application creates a brand-new generation file in a 0x90000-byte
LittleFS partition, closes it via the existing EDP-Persistence
VfsFileStorage, rereads the entire 8192-byte generation-specific
payload, and prints ACK only after checking exact bytes. Only
**AFTER** successful successor creation/ACK does it remove the file
that has just become retired (generation N–2). Thus two most recent
records are protected throughout successor creation. It uses the
already-existing FileStorage abstract API; no new Logging flash driver
or duplicated FileStorage method implementation.

The experiment writes up to 64 unique file names and deletes
retired names. The disk occupancy should stay at approximately 2–4
individual files (including a possibly incomplete successor and
possibly interrupted retired file). After an actual GPIO17
hard-electrical power cut during the reclamation campaign, the MCU
must observe ESP_RST_POWERON, scan the finite named sequence and
recover two individually checked, adjacent newest-generation files.
A third stale file may exist if interrupted deletion did not complete.
Previously retired generations are NOT entitled to durability.

Tests report minimum and current native free heap so the LittleFS
mount and bounded archive's runtime resources can be examined.
This fixture is not a production EDLS archive; it does not cover
every native flash/GC phase or endurance/erase budget. It is not
permission to upgrade the generic FileStorage Restart/None profile.

Safety: Before flashing/provisioning a dedicated test-only partition,
the approved host runner preserves a private full **2 MiB chip image**,
then independently flashes/verifies the application and a separately
generated seeded filesystem (EDP1). The mount does not autoformat.
Only the Policy-approved RPI400 esp32-integrated daemon handles
GPIO17; the test runner leaves VDD OFF on every return. Existing user
wiring confirms true power cut with no back-power.

RPI400 evidence is private. Do not publish raw UART, full flash
images, compiled binaries or other hardware contents to GitHub.
All EDP versions remain 0.1.0, Mesh untouched.

## RPI400 strict electrical-cut evidence: 2026-10-23

The previous run 20261023T174259-78faa008 FAIL was not POR-qualified
because the reset reason was SW_RESET(3); native app correctly halted.
The independent private 589824-byte postcut LittleFS partition contained
complete g0024.bin and g0025.bin (8192 bytes each). Their presence
did not override the reset failure.

Corrected seven C++ telemetry markers that emitted literal backslash-n
rather than newline, and held the GPIO17 relay OFF for at least 8 seconds
with independent controller status checks. The monitor now accepts a
test-selected 'reclaim' or 'write' trigger while preserving the mandatory
ESP_RST_POWERON reset-reason gate.

Genuine electrical-cut run 20261023T215354-859a0db5 PASS exit0:
full pretest 2MiB flash backup sha256
e908c71b9dec7ab5b9b79a7aedb0cda98d608fe8a4cdf924329644d180d6d104,
native ESP-IDF 5.5.5 firmware build 20261023T214810-05fce302 PASS,
factory SHA256 8282c011842512c17da45a330240d973cdd0ac305ecb212a336887b2ce662c7f,
seed partition SHA256 2d0657e9a97bf48c20bec2ac734f7db90438953bce812cbbf1a9c7adcc110ff1,
both written and independently verified after backup. Host cut requested
at RECLAIM_ENTER retired22 after last pre-cut ACK gen24. GPIO17 relay
OFF about 9.140s, ESP32 ROM POWERON_RESET and ESP-IDF ESP_RST_POWERON=1.
After mount without repair, complete 8192-byte protected generations
23 and 24 both independently replayed, valid=2, contiguous=1 and
LAST_CONTIGUOUS_RECOVERED=24 >= last host ACK24. Relay left OFF.
Observed heap after mount 283828B, minimum free heap 278888B in
this fixture, not a WCET or flash-endurance limit.

This qualifies the tested two-generation bounded create/commit/reclaim
protocol for ONE true electrical cut in a reclamation campaign.
It does NOT prove all native erase/GC phases, generic FileStorage
PowerLoss atomicity, capacity exhaustion, or lifetime flash wear.
No original user firmware was restored; private full-chip backups
and serial evidence remain only on RPI400.

## Alternate write-entry hard-cut investigation (same native firmware)

Second test triggered at WRITE_ENTER gen24, after 23 had been
ACKed. RPI400 native test 20261023T220026-4e76a311 FAIL exit1:
GPIO17 controller confirmed supply OFF approximately 9.142 seconds,
then reboot logs exposed BOD/brownout, EXT_SYS_RESET, RTCWDT_RTC_RESET,
with esp_reset_reason() = ESP_RST_WDT(7), NOT the mandatory POR(1).
The native firmware correctly halted; DO NOT mark this a strict
POR-qualified test PASS.

Independent read-only physical partition readback was PASS
20261023T220523-d88d6d99. Its SHA256 was
0affc8a49f896c5f22879767a9517c0babca0e515d7486e02ac4079bd8d1e7ec.
Offline LittleFS extraction found only complete and independently
generation-checked g0023.bin (8192 bytes, all 0x17) and g0024.bin
(8192 bytes, all 0x18), plus EDP1 seed.txt. So both protected records
were actually preserved in the tested medium while reset
classification was not a POR-qualified success. Host command/relay
latency permits successor gen24 completion prior to the actual cut.

All binary/readback/UART evidence remains private on RPI400 under
/srv/espressio/scratch/gpt6-lfs-requal-20261023-2144/
private-bounded-write-POR-20261023/ and related runs directory.
No generic FileStorage interruption-atomicity upgrade is made.

## Extended VDD-OFF diagnostic repetition (16.1 seconds)

Test RPI400 20261023T221520-b4a3dc8c ran the same
WRITE_ENTER gen24 trigger with environment EDP_LFS_OFF_HOLD_S=15.0.
Controller confirmed actual GPIO17 OFF for 16.131 seconds including
status/command overhead. Strict POR gate failed: ESP32 ROM recorded
SDIO_RESET (0x6), application reported ESP_RST_SDIO=10 rather than
ESP_RST_POWERON=1, and app correctly halted NOT_ACTUAL_VDD_CUT.
Do NOT claim POR-qualified hardware PASS.

Independent private postcut 589824-byte physical flash partition
readback run 20261023T222042-c4bdd146 PASS exit0, image SHA256
cb0398b23111fdbce9559096ee54a29e4660811c1fe1066b9af6bcb6c7abee7b.
Offline LittleFS extraction found exactly g0023.bin and g0024.bin,
each 8192 bytes with the expected generation byte repeated throughout,
and seed.txt EDP1. Both protected records survived this experiment;
the difference in reset classification is unresolved.

The host monitor now supports EDP_LFS_OFF_HOLD_S to explicitly
control and log the supply OFF interval. This is a test control only.
The provider continues to advertise conservative Restart/None
atomicity properties. The prior POR-qualified reclaim PASS and
write-stage WDT/SDIO reset failures remain separately classified.
