# ESP-IDF LittleFS VFS binding — conservative qualification

## Objective and architecture

This is the **initial, non-destructive LittleFS integration** for
EDP-Persistence-ESP-IDF. It reuses the established
`VfsFileStorage<TBindingTag,TBindingProfile,TByteOperations>` as
the **sole** FileStorage owner. The new `LittleFsVfsMount` adapter
only calls the upstream ESP-IDF `joltwallet/littlefs` registration
and unregistration APIs. `LittleFsConservativeFileStorage` is an
alias to the existing VFS provider, not another implementation.

The native LittleFS component is an **optional, selected ESP-IDF SDK
dependency**: projects consuming `src/LittleFsVfsMount.hpp` must
provide `joltwallet/littlefs` (component-managed or equivalent).
Projects consuming only VfsFileStorage or NvsKeyValueStorage acquire
no LittleFS dependency. The mount owner uses no heap/allocator,
timer, worker, queue or lock of its own; LittleFS' own mount/wear/cache
allocations are vendor implementation costs to measure on-device.

## Lifecycle and safety

1. Application Bootstrap defines an explicit dedicated flash LittleFS
   partition and obtains stable C strings for its partition label and
   absolute mount root. Both must outlive the mount.
2. Provision/format the partition **only under a separately explicit
   application policy and after safeguarding existing data**.
   LittleFsVfsMount unconditionally sets format_if_mount_failed=false.
   Failure to mount returns ProviderFailure, with no format or repair.
3. Call Mount, check result, then construct the existing
   LittleFsConservativeFileStorage over the same mount root and injected
   EDP-Memory ByteOperations provider. Serialize all mutations while the
   underlying VFS binding advertises CallerSerialized.
4. Destroy dependent FileStorage clients and complete/flush application
   archive operations before calling Unmount. The mount owner is
   noncopyable/nonmovable and attempts cleanup during destruction.

## Deliberately conservative contract

The initial LittleFsConservativeBindingProfile offers Restart retention,
caller-serialized invocation, fixed media, case-sensitive names,
96-byte paths, 63-byte individual segments and a 256-KiB maximum
logical file size. The existing provider **still advertises**:

- FileInterruptionAtomicity = None
- RenameInterruptionAtomicity = None
- AppendInterruptionAtomicity = None
- WriteFileAtInterruptionAtomicity = None
- FileFailurePreservation = MayModify

Upstream LittleFS describes atomic filesystem operations and copy-on-write
power-loss recovery, but the full ESP-IDF VFS, stdio, file sync, mount
and EDLS history path needs **native hardware qualification** before
any stronger Composition property is truthful. The generic provider
already calls fflush, fsync, fclose when given a PowerLoss retention
binding, but the conservative initial profile does **not** assert
PowerLoss and cannot substitute for the NVS-qualified strict journal.

## Tests, physical qualification and limits

`tests/run-littlefs-host-tests.sh` uses a test-only stub of the
exact esp_vfs_littlefs register/unregister API to check invalid
configuration, no-autoformat even on failed mount, lifetime ownership,
cleanup and conservative EDP-Persistence compile-time offers. Execute
with GCC and Clang on the declared RPI400 appliance.

Next qualification must use the real upstream component on a dedicated
preformatted/provisioned partition, full private MCU flash backup,
RPI400 GPIO17 genuine electrical power cuts at multiple phases,
CRC/identity/acknowledgement comparisons across reboots, memory/flash
capacity/wear/write-amplification measurement and independent result
auditing. Successful one-off mount or file rename **does not** prove
that every acknowledged EDLS retained record survives arbitrary
interrupted write, metadata update or garbage collection.

## Negative native power-cut evidence — 2026-10-23

An explicit fully backed-up 2 MiB ESP32 WROOM native test invoked
the **existing** VfsFileStorage::ReplaceFile against the *same*
8 KiB `journal.bin` through a LittleFS VFS mount. A real GPIO17
relay VDD disconnection during active replacements caused
ESP_RST_POWERON on reboot; the LittleFS mount succeeded, but the
formerly ACKed `journal.bin` recovered with **zero bytes**. RPI400
durable run `20261023T134754-502ed2c6` failed its strict check
(exit 1). An independent raw read of the 0x90000 LittleFS partition,
run `20261023T135255-bb9dcbdc` passed, and independent offline
LittleFS extraction confirmed `seed.txt` = "EDP1" and
`journal.bin` length 0. All private bytes are retained only on RPI400.

This is direct *negative* evidence against promoting
FileInterruptionAtomicity for the current FileReplace path.
It does not prove the whole LittleFS filesystem is unsafe, but it
invalidates same-path overwrite of a still-retained record.

A separate, strictly experimental
`tests/hil/littlefs-immutable-espidf/` fixture tests never reusing
a generation filename: each new 8192-byte file is verified before
ACK and all previously ACKed distinct files are independently checked
after reboot. It has no deletion/reclamation, and even a passing cut
cannot establish strict bounded historical retention on its own.
The production provider MUST remain conservative until its *entire*
commit, recovery, exhaustion and reclamation path is proven.


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

## Bounded immutable-history qualification

Dedicated test-only LittleFS fixture tests create-new generation-named
files, independent exact readback before ACK, and delayed deletion
of each retired oldest generation. With correct UART delimiters
and strict reset validation, native WROOM GPIO17 hard-power test
20261023T215354-859a0db5 PASS exit0: POR reboot, protected complete
generations 23 and 24 recovered after about 9.140 seconds VDD-off.
This validates a bounded design candidate, not the generic FileStorage
atomicity contract, garbage-collection phase completeness, or endurance.

An alternate, differently triggered write-entry power-cut attempt,
20261023T220026-4e76a311, did not pass the strict POR gate:
host GPIO17 was OFF for 9.142 sec but the ROM emitted a brownout,
external-reset and RTC watchdog sequence before ESP_RST_WDT(7).
Independent offline physical readback PASS 20261023T220523-d88d6d99
found valid g0023.bin and g0024.bin (8192 bytes each), no protected
record loss observed. This is robustness evidence, NOT a strict
POR-qualified pass, native erase-phase certification or permission
to upgrade generic FileStorage Restart/None guarantees.
