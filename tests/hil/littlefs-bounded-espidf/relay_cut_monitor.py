#!/usr/bin/env python3
"""Bounded LittleFS true VDD hard-cut qualification host.

Electrical provenance is exclusively supplied by the user-qualified RPI400
GPIO17 controller. Device ROM/application reset reason is diagnostic only.
Never claim FileStorage::ReplaceFile power-loss atomicity from this test.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import serial
import subprocess
import sys
import time

from powercut_qualification import PostCutEvidence

CONTROL = "/srv/espressio/bin/esp32-integrated"


def command(value: str, *, expected_power: str | None = None) -> str:
    completed = subprocess.run(
        [CONTROL, value], check=True, capture_output=True,
        text=True, timeout=12,
    )
    response = completed.stdout.strip()
    print(f"HOST|{value}|{response}", flush=True)
    if expected_power is not None and f"POWER={expected_power}" not in response:
        raise RuntimeError(
            f"{value}: expected controller POWER={expected_power}: {response}"
        )
    return response


def run(destination: str) -> int:
    evidence_path = Path(destination)
    evidence_path.mkdir(parents=True, exist_ok=False)

    stage = os.environ.get("EDP_LFS_CUT_STAGE", "reclaim").lower()
    if stage not in {"reclaim", "write", "lowspace"}:
        raise ValueError("cut stage must be reclaim, write or lowspace")
    threshold = 22 if stage == "reclaim" else (24 if stage == "write" else 1)
    trigger = (
        re.compile(r"\|RECLAIM_ENTER\|retired=(\d+)")
        if stage == "reclaim"
        else (
            re.compile(r"\|WRITE_ENTER\|gen=(\d+)")
            if stage == "write"
            else re.compile(r"\|LOWSPACE_SAFE\|protected=1,2\|last_ack=(\d+)")
        )
    )
    off_hold_s = float(os.environ.get("EDP_LFS_OFF_HOLD_S", "8.0"))
    if not (5.0 <= off_hold_s <= 60.0):
        raise ValueError("qualification OFF hold must be within [5,60] seconds")

    print(
        f"HOST|CONFIGURED_CUT_STAGE={stage}|threshold={threshold}"
        f"|min_off_seconds={off_hold_s}",
        flush=True,
    )
    last_ack = 0
    cut_triggered = False
    postcut_evidence: PostCutEvidence | None = None
    off_confirmed = False
    on_confirmed = False
    measured_off_s = 0.0
    started = time.monotonic()

    # The official flashing wrapper may have already powered ON and held
    # reset; a monitor-only replay can begin powered OFF. Only actual cut
    # OFF/ON controller witnesses are strict.
    initial_status = command("status")
    if "POWER=OFF" not in initial_status and "POWER=ON" not in initial_status:
        raise RuntimeError("initial controller power state unrepresentable")
    port = serial.Serial("/dev/serial0", 115200, timeout=0.1)
    pending = bytearray()
    try:
        port.reset_input_buffer()
        with (evidence_path / "serial.raw").open("wb") as raw, (
            evidence_path / "serial.txt"
        ).open("w", encoding="utf8") as log:
            command("power-on", expected_power="ON")
            command("release", expected_power="ON")
            while time.monotonic() - started < 110.0:
                data = port.read(4096)
                if not data:
                    continue
                raw.write(data)
                raw.flush()
                for octet in data:
                    if octet != 10 and len(pending) < 4095:
                        pending.append(octet)
                        continue

                    line = pending.decode("utf8", errors="replace").strip()
                    pending.clear()
                    log.write(line + "\n")
                    log.flush()
                    if postcut_evidence is not None:
                        postcut_evidence.observe(line)

                    if "EDP_BND|" in line:
                        frame = line[line.index("EDP_BND|"):]
                        print("UART|" + frame, flush=True)
                        if not cut_triggered:
                            m = re.search(r"\|ACK\|gen=(\d+)", frame)
                            if m:
                                last_ack = max(last_ack, int(m.group(1)))

                    if postcut_evidence is not None:
                        if (
                            postcut_evidence.replay_verified
                            or postcut_evidence.halt_reason is not None
                        ):
                            verdict = postcut_evidence.verdict(
                                ack_before_cut=last_ack,
                                off_confirmed=off_confirmed,
                                on_confirmed=on_confirmed,
                                measured_off_seconds=measured_off_s,
                                minimum_off_seconds=off_hold_s,
                                require_rolling=stage == "lowspace",
                            )
                            print(
                                f"HOST|QUALIFICATION={verdict.classification}"
                                f"|accepted={verdict.accepted}"
                                f"|failure={verdict.failure}"
                                f"|last_ack={last_ack}"
                                f"|newest={verdict.recovered_newest}"
                                f"|older={verdict.recovered_older}"
                                f"|valid={verdict.recovered_count}"
                                f"|app_reset={verdict.native_reset}"
                                f"|rom_resets={verdict.rom_resets}"
                                f"|off_s={measured_off_s:.3f}",
                                flush=True,
                            )
                            return 0 if verdict.accepted else 4
                        continue

                    if "EDP_BND|HALT|" in line:
                        print("HOST|PRE_CUT_FIRMWARE_HALT=FAIL", flush=True)
                        return 5

                    if not cut_triggered:
                        m = trigger.search(line)
                        if m and int(m.group(1)) >= threshold:
                            if last_ack < 2:
                                raise RuntimeError(
                                    "not enough acknowledged baseline records"
                                )
                            print(
                                f"HOST|CUT_STAGE={stage}"
                                f"|CUT_AT={m.group(1)}"
                                f"|LAST_ACK={last_ack}",
                                flush=True,
                            )
                            cut_triggered = True
                            command("power-off", expected_power="OFF")
                            off_start = time.monotonic()
                            command("status", expected_power="OFF")
                            time.sleep(off_hold_s)
                            command("status", expected_power="OFF")
                            measured_off_s = time.monotonic() - off_start
                            off_confirmed = True

                            # Eliminate any UART bytes buffered before this
                            # NEW power-on. The remainder of the already-read
                            # data chunk is discarded by breaking this loop.
                            port.reset_input_buffer()
                            pending.clear()
                            command("power-on", expected_power="ON")
                            on_confirmed = True
                            postcut_evidence = PostCutEvidence()
                            print(
                                f"HOST|POWER_OFF_S={measured_off_s:.3f}"
                                f"|new_postcut_boot_epoch=1",
                                flush=True,
                            )
                            break
            print(
                f"HOST|TIMEOUT|cut={cut_triggered}"
                f"|last_ack={last_ack}|postcut={postcut_evidence is not None}",
                flush=True,
            )
            return 6
    finally:
        port.close()
        command("power-off", expected_power="OFF")


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("unique private evidence path required")
        sys.exit(run(sys.argv[1]))
    except Exception as exc:
        print(f"HOST|ERROR|{exc!r}", flush=True)
        sys.exit(9)
