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

## 2026-10-24 approved external VDD acceptance policy

The user approved **separately classified, controller-verified power
interruption** even when ESP32 reports WDT, brownout or SDIO reset.
See Policy/main/V2/TEST_COVERAGE.MD, section 4. Do not rewrite or
relabel original historical POR-only run verdicts.

This fixture no longer treats a reset reason as electrical-cut proof:
the firmware preserves EDP_BND BOOT/RESET_OBSERVED diagnostics, scans
all 64 finite candidate names through the existing FileStorage,
verifies two adjacent complete latest retained records and emits only
REPLAY_VERIFIED, never a powercut-success assertion.

The independent RPI400 monitor demands controller-confirmed GPIO17
OFF and ON, minimum measured OFF hold, a post-cut boot epoch (not
buffered pre-cut UART), complete readback/replay and no loss of the
protected window relative to the last pre-cut acknowledged record.
It reports POR_CONFIRMED only if both native ESP_RST_POWERON and
raw ROM POWERON_RESET are observed in that epoch, otherwise
EXTERNALLY_VERIFIED_POWER_CUT. Unknown/missing observations remain
FAIL, never an implicit pass. Raw ROM and app reset reasons are
preserved even in non-POR cases. Boot replay is NOT an electrical
measurement or a FileStorage PowerLoss property. Host checks are
covered by 19 deterministic tests in test_powercut_qualification.py.

Be aware that the test-only eight-second (or explicit configured)
OFF interval measures controller-confirmed relay OFF and uses the
human-verified no-backpower wiring; it does not provide an independent
analog oscilloscope voltage trace. Physical evidence quality should be
described precisely.

## 2026-10-24 independent POR-confirmed GPIO17 hard-cut replay

Approved V2/TEST_COVERAGE powercut semantics are now implemented
strictly: firmware logs actual ROM/app reset reason and protected
history replay only; RPI400 independently attests controller OFF/ON,
elapsed hold and post-cut boot epoch before classifying the result.

RPI400 native ESP-IDF 5.5.5 build 20261024T092051-10b1cf1f PASS0.
First physical runner 20261024T092844-f429f9fe stopped fail-closed
during monitor preflight (wrapper had POWER=ON while holding reset,
but monitor initially demanded OFF), without running application/cut.
It nevertheless first preserved a verified complete 2 MiB flash backup
SHA256 7320c35b1928c52fe2269684de9a9160ac2dc6ee71c9bdc7e9bbe22e8293fc44
and independently flashed/verified factory firmware image SHA256
4a0b255aa97e986df6a1b5c0da9c812f6a2e6535e1d46009e9207019f8ba4942
and dedicated 589824-byte seeded LittleFS partition SHA256
2d0657e9a97bf48c20bec2ac734f7db90438953bce812cbbf1a9c7adcc110ff1.
Preflight fixed to accept either initial controller power state;
strict OFF/ON evidence required during the real cut. The unchanged
verified image and backup were reused instead of reflashing.

Actual GPIO17 WRITE_ENTER gen24 monitor 20261024T093434-c39effc2
PASS0: host last ACK gen23; OFF=8.273 seconds and GPIO17 OFF status
verified; new raw ROM POWERON_RESET and native reset 1; independently
read/recovered complete g23/g24 and an extra fully valid g22 (retirement
may lag completion of newest generation); app REPLAY_VERIFIED only.
RPI400 host categorised POR_CONFIRMED, accepted=True, with raw reset
reason preserved; GPIO17 OFF after test.

Independent *physical* post-cut flash-partition readback
20261024T093534-7f3f8754 PASS0, image SHA256
f210474f2fe26984454ed7a54d01e4e8d932c52c896c7a73ba55bb47335fbd3c,
offline LittleFS extracted exactly g22, g23, g24 (8192 bytes,
generation-specific patterns entirely correct) plus EDP1 seed.
This is an independent medium-validation receipt, not merely a
firmware log assertion. Full backup, firmware, partition and raw UART
remain private on RPI400; do not put them into GitHub.

All 22 pure qualification and five end-to-end emulated control/UART
tests passed, RPI400 20261024T093356-b4e3ccca. Unchanged GCC+Clang
LittleFS provider/mount host contracts passed, run
20261024T092926-d9b2417e. No in-place replacement durability
claim, no automatic recovery repair, no flash-wear or GC-phase proof,
no production EDLS historical archive claim.

## 2026-10-26: Opt-in high-water/exhaustion power-cut fixture

The separate `esp32dev-lowspace` profile is **test-only**. It retains
exactly two acknowledged 8192-byte immutable generations (g0001.bin and
g0002.bin), tests duplicate-name preflight refusal without mutating the
existing path, and consumes the dedicated 0x90000 LittleFS partition via
finite **unprotected synthetic padding** (maximum 96 distinct 8192-byte
files). It then observes a real provider write failure for *unacknowledged*
g0003.bin. It NEVER announces ACK(3) on failure, NEVER deletes g1/g2, and
verifies both protected generations before declaring LOWSPACE_SAFE. If
physical exhaustion or the expected failure does not actually occur, the
fixture halts rather than fabricating a success. A torn g3 is permitted
because it never became acknowledged. On reboot the firmware independently
replays and checks the protected g1/g2 window, even if an entirely valid
unacknowledged candidate happens to be present. The host continues to own
the electrical-cut verdict, and requires actual controller OFF/ON and a new
boot epoch. The logical capacity is finite; no indefinite retention promise
is made. Filler files are NOT log records, not a retained archive, and
not a production allocator; the test is not yet qualified GC phase coverage.

Run this *only* on the user-authorised, locked RPI400, after the native
lowspace image is built and individually confirmed, with explicit
`EDP_LFS_CUT_STAGE=lowspace EDP_LFS_BUILD_ENV=esp32dev-lowspace`. The
existing full-chip/private backup and preflash image verification wrapper
is reused unchanged. Both options are required together to prevent
accidental mismatch with the previously qualified normal write/reclaim
firmware. The fixture must leave relay POWER=OFF and never autoformat on
mount. Retain the full physical flash image privately for offline
protected-window validation. Do not upgrade generic PowerLoss traits.

The existing `esp32dev` environment and normal write/reclaim tests are
unchanged. Host negative GPIO17/test-control coverage includes insufficient
retained replay, and failed controller OFF evidence for the lowspace stage.

**Recovery and finite rotation:** After controller-triggered failure and a
verified new boot, the fixture first revalidates g1 and g2 while the
filesystem remains near-full. It then deletes ONLY disposable filler and
the unacknowledged g3 candidate (if present), verifies g1/g2 again, creates
and independently re-reads a brand-new g3, emits ACK3, and only then
retires g1. It verifies the resulting exact bounded g2/g3 window and
emits `LOWSPACE_ROLLED`. The host's lowspace power-cut verdict requires
this marker, not just an unchanged old two-file replay. It does not make
logs indefinite, prove uninterrupted arbitrary GC, or provide a production
EDLS allocator/retention policy.

**Independent raw-media evidence:** after the finished GPIO17 test,
separately dump partition `0x170000 + 0x90000` into a unique private
backup path, use the already-installed `littlefs-python extract --block-size
4096` to extract it read-only, then run
`python3 audit_lowspace_partition.py <private-extracted-dir>`. This
requires ONLY seed.txt plus the checked complete g0002.bin and g0003.bin,
and no retired g0001.bin or disposable pad files. The checker tests finite
synthetic payload patterns; it does NOT claim canonical EDLS/EDLG or wear
qualification. Private extracted images and UART remain on RPI400.
