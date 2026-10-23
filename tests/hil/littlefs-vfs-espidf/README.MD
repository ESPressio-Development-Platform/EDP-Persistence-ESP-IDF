# LittleFS VFS native ESP32 power-interruption qualification

This test uses **real** ESP-IDF 5.5.5 with managed
joltwallet/littlefs **1.22.3** (third-party SDK dependency, not an EDP
library version), the new domain-specific LittleFsVfsMount owner, and
the **pre-existing** EDP-Persistence-ESP-IDF VfsFileStorage. No second
FileStorage implementation is created. The ordinary binding deliberately
advertises Restart retention and interrupted-operation None.

The RPI400 integrated ESP32-WROOM-32 has a user-confirmed genuine VDD
GPIO17 relay cutoff with no back-power paths. Only the approved
esp32-integrated controller handles GPIO17, ROM GPIO and serial reset.
The test never manipulates relay pins or GPIO directly.

## Safe explicit provisioning

The runner first stores a private **complete 2 MiB device flash backup**,
then flashes and independently verifies the firmware factory image. It
**separately provisions only test-specific partition `edplfs`** at
flash offset 0x170000 with an offline-generated seeded LittleFS image
of length 0x90000 (589,824 bytes) before booting the test firmware.
The seed content is exactly `EDP1` in `seed.txt`, generated using
littlefs-python (4096-byte native erase blocks). Provisioning is
visible, explicit, and protected by the pretest backup; the production
`LittleFsVfsMount` itself **never formats or repairs** anything.

## Observed power-cut protocol

Application mounts LittleFS, rejects any unknown seed or phase,
writes a synced-through-close 8192-byte baseline generation and
independently reads the full file. It records an NVS phase marker
and prints `ACK|gen=1`, then performs repeated, full-file
`VfsFileStorage::ReplaceFile` updates with generation-specific
all-byte patterns. RPI400 captures greatest host-observed ACK,
switches real VDD OFF on a subsequent WRITE_ENTER, restores it, and
requires ESP_RST_POWERON. The actual recovered file must be exactly
8192 bytes and **every byte must equal the recovered generation**.
The host additionally requires generation >= its last visible ACK.

This is a single-file durability *experiment*, NOT a guarantee
that all LittleFS file operations are PowerLoss atomic or that EDLS
historical retention/reclamation is complete. The host/relay latency
may mean the cut lands after the current native write finishes.
Multiple randomized cuts at different native phases, metadata/rename
operations and resource/wear profiling remain release requirements.

## Running

Select/lock RPI400 according to Policy/main/V2. Build this ESP-IDF
PIOArduino project with the pinned EDP siblings. Generate the
littlefs-python seed image and set a unique private
`EDP_LITTLEFS_EVIDENCE_DIR` and accurate exclusive owner ID
`EDP_LITTLEFS_AGENT_ID`. Run `run_relay_cut_test.sh` inside
`espressio-run-detached`, preserving the returned durable run ID.
The runner traps every outcome to leave GPIO17 relay OFF.
Never upload raw flash images, private logs or binary firmware to GitHub.

All EDP library versions remain **0.1.0**.

## Native outcome — rejected atomic-replacement claim

Run `20261023T134754-502ed2c6` correctly failed after GPIO17
power-cut recovery found `journal.bin` invalid. Independent private
partition readback/extraction `20261023T135255-bb9dcbdc` recovered
`journal.bin` as exactly **0 bytes**, while `seed.txt` remained
EDP1. Do not overwrite previously retained records with in-place
ReplaceFile. See the separate immutable-generation experiment.
