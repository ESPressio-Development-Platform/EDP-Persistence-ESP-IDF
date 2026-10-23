#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_LITTLEFS_AGENT_ID:?Exclusive RPI400 lock-owner ID required}"
EVIDENCE="${EDP_LITTLEFS_EVIDENCE_DIR:?Unique private absolute evidence path required}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
PROJECT="$ROOT/tests/hil/littlefs-bounded-espidf"
IMAGE="$PROJECT/.pio/build/esp32dev/firmware.factory.bin"
FS_IMAGE="$PROJECT/.pio/littlefs-seeded.img"
CONTROL=/srv/espressio/bin/esp32-integrated
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test -s "$IMAGE"
test -s "$FS_IMAGE"
test "$(stat -c %s "$FS_IMAGE")" -eq 589824
test ! -e "$EVIDENCE"
mkdir -p "$EVIDENCE"
sha256sum "$IMAGE" "$FS_IMAGE" > "$EVIDENCE/test-images.sha256"
"$CONTROL" status
/srv/espressio/bin/rpi400-status flash 'LittleFS bounded' 'full chip backup then explicit test provisioning'
"$CONTROL" esptool read-flash 0x0 0x200000 "$EVIDENCE/backup-2mb.bin" > "$EVIDENCE/backup.log" 2>&1
test "$(stat -c %s "$EVIDENCE/backup-2mb.bin")" -eq 2097152
sha256sum "$EVIDENCE/backup-2mb.bin" > "$EVIDENCE/backup.sha256"
echo FULL_CHIP_BACKUP=PASS
"$CONTROL" esptool write-flash 0x0 "$IMAGE" > "$EVIDENCE/flash-firmware.log" 2>&1
"$CONTROL" esptool verify-flash 0x0 "$IMAGE" > "$EVIDENCE/verify-firmware.log" 2>&1
echo FIRMWARE_VERIFIED=PASS
# EXPLICIT TEST PROVISIONING: only this dedicated LittleFS partition;
# full-device backup already completed. Production mount NEVER autoformats.
"$CONTROL" esptool write-flash 0x170000 "$FS_IMAGE" > "$EVIDENCE/flash-littlefs.log" 2>&1
"$CONTROL" esptool verify-flash 0x170000 "$FS_IMAGE" > "$EVIDENCE/verify-littlefs.log" 2>&1
echo DEDICATED_LITTLEFS_IMAGE_VERIFIED=PASS
"$CONTROL" hold-reset
/srv/espressio/bin/rpi400-status serial 'LittleFS bounded' 'GPIO17 hard power recovery'
set +e
python3 -u "$PROJECT/relay_cut_monitor.py" "$EVIDENCE/monitor"
rc=$?
set -e
"$CONTROL" status
echo "LITTLEFS_MONITOR_EXIT=$rc"
test "$rc" -eq 0
echo LITTLEFS_BOUNDED_RECLAMATION_EVALUATION=PASS
