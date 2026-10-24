#!/usr/bin/env python3
"""Deterministic host-side qualification regression matrix; no hardware access."""
import unittest

from powercut_qualification import PostCutEvidence


def evidence(*, reason=1, rom="rst:0x1 (POWERON_RESET)",
             newest=24, older=23, valid=2, replay=True):
    state = PostCutEvidence()
    if rom:
        state.observe(rom)
    if reason is not None:
        state.observe(f"EDP_BND|BOOT|reset={reason}")
        state.observe(
            f"EDP_BND|RESET_OBSERVED|reason={reason}"
            f"|por={1 if reason == 1 else 0}"
        )
    if newest is not None:
        state.observe(
            f"EDP_BND|RECOVERED|newest={newest}|older={older}"
            f"|valid={valid}|contiguous=1"
        )
    if replay:
        state.observe("EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT")
    return state


def verdict(state=None, **kw):
    args = dict(ack_before_cut=24, off_confirmed=True, on_confirmed=True,
                measured_off_seconds=8.2, minimum_off_seconds=8.0)
    args.update(kw)
    return (state or evidence()).verdict(**args)


class ExternalVddQualificationTests(unittest.TestCase):
    def test_native_por_and_rom_por_is_strict_subtype(self):
        v = verdict()
        self.assertTrue(v.accepted)
        self.assertEqual(v.classification, "POR_CONFIRMED")

    def test_watchdog_reset_is_external_cut_not_por(self):
        v = verdict(evidence(reason=7, rom="rst:0x10 (RTCWDT_RTC_RESET)"))
        self.assertTrue(v.accepted)
        self.assertEqual(v.classification, "EXTERNALLY_VERIFIED_POWER_CUT")
        self.assertEqual(v.native_reset, 7)

    def test_sdio_reset_is_external_cut_not_por(self):
        v = verdict(evidence(reason=10, rom="rst:0x6 (SDIO_RESET)"))
        self.assertTrue(v.accepted)
        self.assertEqual(v.classification, "EXTERNALLY_VERIFIED_POWER_CUT")

    def test_missing_rom_boot_diagnostic_fails_closed(self):
        v = verdict(evidence(reason=7, rom=None))
        self.assertEqual(v.failure, "POSTCUT_ROM_RESET_NOT_OBSERVED")

    def test_missing_application_reset_detail_fails(self):
        v = PostCutEvidence()
        v.observe("rst:0x6 (SDIO_RESET)")
        v.observe("EDP_BND|BOOT|reset=10")
        v.observe("EDP_BND|RECOVERED|newest=24|older=23|valid=2|contiguous=1")
        v.observe("EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT")
        self.assertEqual(verdict(v).failure,
                         "APPLICATION_RESET_DIAGNOSTICS_INCONSISTENT")

    def test_mismatched_native_reset_detail_fails(self):
        v = evidence(reason=10, rom="rst:0x6 (SDIO_RESET)")
        v.observe("EDP_BND|RESET_OBSERVED|reason=1|por=1")
        self.assertEqual(verdict(v).failure,
                         "APPLICATION_RESET_DIAGNOSTICS_INCONSISTENT")

    def test_native_por_without_rom_proof_not_strict_por(self):
        v = verdict(evidence(reason=1, rom="rst:0x6 (SDIO_RESET)"))
        self.assertTrue(v.accepted)
        self.assertEqual(v.classification, "EXTERNALLY_VERIFIED_POWER_CUT")

    def test_missing_controller_off(self):
        v = verdict(off_confirmed=False)
        self.assertFalse(v.accepted)
        self.assertEqual(v.failure, "CONTROLLER_OFF_ON_NOT_VERIFIED")

    def test_missing_controller_on(self):
        self.assertFalse(verdict(on_confirmed=False).accepted)

    def test_off_interval_too_short(self):
        v = verdict(measured_off_seconds=2.0)
        self.assertEqual(v.failure, "OFF_DURATION_UNQUALIFIED")

    def test_insufficient_precut_records(self):
        v = verdict(ack_before_cut=1)
        self.assertEqual(v.failure, "INSUFFICIENT_PRECUT_BASELINE")

    def test_boot_from_new_epoch_missing(self):
        v = verdict(evidence(reason=None))
        self.assertEqual(v.failure, "POSTCUT_BOOT_NOT_OBSERVED")

    def test_pre_cut_replay_cannot_be_reused(self):
        new_epoch = PostCutEvidence()
        new_epoch.observe("rst:0x10 (RTCWDT_RTC_RESET)")
        new_epoch.observe("EDP_BND|BOOT|reset=7")
        new_epoch.observe("EDP_BND|RESET_OBSERVED|reason=7|por=0")
        v = verdict(new_epoch)
        self.assertEqual(v.failure, "PROTECTED_REPLAY_NOT_CONFIRMED")

    def test_missing_validated_replay(self):
        v = verdict(evidence(replay=False))
        self.assertEqual(v.failure, "PROTECTED_REPLAY_NOT_CONFIRMED")

    def test_missing_recovered_window(self):
        v = verdict(evidence(newest=None))
        self.assertEqual(v.failure, "RETAINED_WINDOW_NOT_OBSERVED")

    def test_newest_less_than_precut_ack(self):
        v = verdict(evidence(newest=23, older=22))
        self.assertEqual(v.failure, "PROTECTED_RETAINED_WINDOW_FAILED")

    def test_non_adjacent_protected_files(self):
        v = verdict(evidence(newest=24, older=22))
        self.assertEqual(v.failure, "PROTECTED_RETAINED_WINDOW_FAILED")

    def test_wrong_valid_count(self):
        v = verdict(evidence(valid=1))
        self.assertEqual(v.failure, "PROTECTED_RETAINED_WINDOW_FAILED")

    def test_newer_unacknowledged_complete_generation_allowed(self):
        v = verdict(evidence(newest=25, older=24))
        self.assertTrue(v.accepted)

    def test_firmware_halt_fails_closed(self):
        state = evidence(reason=7, rom="rst:0x10 (RTCWDT_RTC_RESET)")
        state.observe("EDP_BND|HALT|INVALID_SEED")
        v = verdict(state)
        self.assertEqual(v.failure, "FIRMWARE_HALT:INVALID_SEED")

    def test_multiple_boots_in_single_postcut_epoch_are_ambiguous(self):
        state = evidence()
        state.observe("EDP_BND|BOOT|reset=10")
        self.assertEqual(
            verdict(state).failure,
            "FIRMWARE_HALT:MULTIPLE_POSTCUT_BOOT_EPOCHS",
        )

    def test_raw_reset_sequence_preserved(self):
        state = evidence(reason=7, rom="rst:0x2 (EXT_SYS_RESET)")
        state.observe("rst:0x10 (RTCWDT_RTC_RESET)")
        v = verdict(state)
        self.assertTrue(v.accepted)
        self.assertEqual(v.rom_resets, (
            (2, "EXT_SYS_RESET"), (16, "RTCWDT_RTC_RESET"),
        ))


if __name__ == "__main__":
    unittest.main()
