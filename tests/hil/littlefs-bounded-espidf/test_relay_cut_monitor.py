#!/usr/bin/env python3
"""Fully offline GPIO17 host-control/serial-buffer replay regression tests."""
from __future__ import annotations

import io
import os
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import relay_cut_monitor as monitor


class EmulatedSerial:
    def __init__(self, pre_cut: bytes, post_cut: bytes):
        self.pre_cut = pre_cut
        self.post_cut = post_cut
        self.power_on_count = 0
        self.powered = False
        self.initial_done = False
        self.recovery_done = False
        self.reset_calls = 0
        self.closed = False

    def read(self, _maximum):
        if self.power_on_count >= 2 and not self.recovery_done:
            self.recovery_done = True
            return self.post_cut
        if self.power_on_count == 1 and not self.initial_done:
            self.initial_done = True
            return self.pre_cut
        return b""

    def reset_input_buffer(self):
        self.reset_calls += 1

    def close(self):
        self.closed = True


class ElectricalControllerAndBufferTests(unittest.TestCase):
    def exercise(self, *, native=7, rom="rst:0x10 (RTCWDT_RTC_RESET)",
                 wrong_off=False, bad_recovery=False,
                 lowspace=False, lowspace_reclaim=False):
        # Last line of the original UART chunk is STALE pre-cut replay.
        # The event loop MUST discard it when a new power epoch begins.
        before = (
            b"rst:0x1 (POWERON_RESET)\n"
            b"EDP_BND|BOOT|reset=1\n"
            b"EDP_BND|ACK|gen=23\n"
            b"EDP_BND|WRITE_ENTER|gen=24\n"
            b"EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
        )
        after = (
            f"{rom}\n"
            f"EDP_BND|BOOT|reset={native}\n"
            f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
            + (
                "EDP_BND|RECOVERED|newest=22|older=21|valid=2|contiguous=1\n"
                if bad_recovery else
                "EDP_BND|RECOVERED|newest=24|older=23|valid=2|contiguous=1\n"
            )
            + "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
        ).encode()
        if lowspace or lowspace_reclaim:
            before = (
                b"rst:0x1 (POWERON_RESET)\n"
                b"EDP_BND|BOOT|reset=1\n"
                b"EDP_BND|ACK|gen=2\n"
                b"EDP_BND|LOWSPACE_SAFE|protected=1,2|last_ack=2|pads=59|no_ack_3=1\n"
                + (b"EDP_BND|LOWSPACE_RECLAIM_ENTER|pad=5\n"
                   if lowspace_reclaim else b"")
            )
            after = (
                f"{rom}\n"
                f"EDP_BND|BOOT|reset={native}\n"
                f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
                + (
                    "EDP_BND|RECOVERED|newest=2|older=0|valid=1|contiguous=1\n"
                    if bad_recovery else
                    "EDP_BND|RECOVERED|newest=2|older=1|valid=2|contiguous=1\n"
                )
                + ("" if bad_recovery else
                   "EDP_BND|LOWSPACE_ROLLED|ack=3|window=2,3|retired=1\n")
                + "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
            ).encode()
        port = EmulatedSerial(before, after)
        elapsed = [0.0]
        actual_clock = time.monotonic

        def emulate_command(args, **_kwargs):
            action = args[-1]
            if action == "power-on":
                port.power_on_count += 1
                port.powered = True
            if action == "power-off":
                port.powered = False
            state = "ON" if port.powered else "OFF"
            if wrong_off and action == "power-off":
                state = "ON"
            return SimpleNamespace(stdout=f"OK {action} POWER={state}\n")

        def emulate_sleep(seconds):
            elapsed[0] += seconds + 0.1

        results = []
        with (
            tempfile.TemporaryDirectory() as td,
            patch.object(monitor.serial, "Serial", return_value=port),
            patch.object(monitor.subprocess, "run", side_effect=emulate_command),
            patch.object(monitor.time, "sleep", side_effect=emulate_sleep),
            patch.object(monitor.time, "monotonic",
                         side_effect=lambda: actual_clock()+elapsed[0]),
            patch.dict(os.environ, {
                "EDP_LFS_CUT_STAGE": ("lowspace_reclaim" if lowspace_reclaim else
                                      "lowspace" if lowspace else "write"),
                "EDP_LFS_OFF_HOLD_S": "8.0",
            }),
            patch("builtins.print", side_effect=lambda *a, **_k: results.append(
                " ".join(map(str, a))
            )),
        ):
            try:
                exit_code = monitor.run(str(Path(td)/"evidence"))
            except RuntimeError:
                exit_code = 9
            recorded = (Path(td)/"evidence"/"serial.txt").read_text()
        return exit_code, results, recorded, port

    def test_real_por_from_new_epoch(self):
        code, output, recorded, port = self.exercise(
            native=1, rom="rst:0x1 (POWERON_RESET)"
        )
        self.assertEqual(code, 0)
        self.assertTrue(any("QUALIFICATION=POR_CONFIRMED" in x for x in output))
        self.assertEqual(port.reset_calls, 2)
        self.assertTrue(port.closed)

    def test_watchdog_physically_accepted_not_por(self):
        code, output, _, _ = self.exercise()
        self.assertEqual(code, 0)
        self.assertTrue(any(
            "QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT" in x
            for x in output
        ))

    def test_sdio_physically_accepted_not_por(self):
        code, output, _, _ = self.exercise(
            native=10, rom="rst:0x6 (SDIO_RESET)"
        )
        self.assertEqual(code, 0)
        self.assertTrue(any("app_reset=10" in x for x in output))

    def test_precut_uart_replay_is_not_sufficient(self):
        code, _, recorded, _ = self.exercise(bad_recovery=True)
        self.assertEqual(code, 4)
        # Precut replay marker in the buffered chunk was discarded.
        self.assertEqual(recorded.count("REPLAY_VERIFIED"), 1)

    def test_lowspace_failed_successor_preserves_acknowledged_pair(self):
        code, output, _, _ = self.exercise(lowspace=True)
        self.assertEqual(code, 0)
        self.assertTrue(any("QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT"
                            in x for x in output))
        self.assertTrue(any("last_ack=2|newest=2|older=1"
                            in x for x in output))

    def test_lowspace_corrupt_protected_pair_is_rejected(self):
        code, output, _, _ = self.exercise(
            lowspace=True, bad_recovery=True
        )
        self.assertEqual(code, 4)
        self.assertTrue(any("PROTECTED_RETAINED_WINDOW_FAILED"
                            in x for x in output))

    def test_lowspace_controller_failure_is_rejected(self):
        code, _, _, _ = self.exercise(
            lowspace=True, wrong_off=True
        )
        self.assertEqual(code, 9)

    def test_lowspace_reclaim_cut_and_complete_verified_roll(self):
        code, output, _, _ = self.exercise(lowspace_reclaim=True)
        self.assertEqual(code, 0)
        self.assertTrue(any(
            "CUT_STAGE=lowspace_reclaim|CUT_AT=5|LAST_ACK=2" in x
            for x in output
        ))
        self.assertTrue(any(
            "QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT" in x
            for x in output
        ))

    def test_lowspace_reclaim_cut_rejects_corrupt_protected_window(self):
        code, output, _, _ = self.exercise(
            lowspace_reclaim=True, bad_recovery=True
        )
        self.assertEqual(code, 4)
        self.assertTrue(any(
            "PROTECTED_RETAINED_WINDOW_FAILED" in x for x in output
        ))

    def test_controller_did_not_confirm_relay_off(self):
        code, _, _, _ = self.exercise(wrong_off=True)
        self.assertEqual(code, 9)


if __name__ == "__main__":
    unittest.main()
