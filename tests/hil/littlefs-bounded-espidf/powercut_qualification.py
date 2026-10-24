"""Pure, deterministic classification of verified GPIO17 power-cut evidence.

Only the host owning the relay may assert a VDD cut. A firmware reset
reason, however reported, is diagnostic rather than the power-cut oracle.
"""
from __future__ import annotations

from dataclasses import dataclass
import re

BOOT = re.compile(r"\bEDP_BND\|BOOT\|reset=(\d+)\b")
RECOVER = re.compile(
    r"\bEDP_BND\|RECOVERED\|newest=(\d+)\|older=(\d+)"
    r"\|valid=(\d+)\|contiguous=1\b"
)
ROM_RESET = re.compile(r"\brst:0x([0-9a-fA-F]+)\s*\(([^)]*)\)")
RESET_OBSERVED = re.compile(
    r"\bEDP_BND\|RESET_OBSERVED\|reason=(\d+)\|por=([01])\b"
)
REPLAY_PASS = "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT"


@dataclass(frozen=True)
class CutVerdict:
    accepted: bool
    classification: str
    failure: str
    native_reset: int | None
    rom_resets: tuple[tuple[int, str], ...]
    recovered_newest: int | None
    recovered_older: int | None
    recovered_count: int | None


class PostCutEvidence:
    """Only feed data gathered after the *new* power-on.

    Host caller flushes UART's pre-cut input before powering up, and starts
    a new PostCutEvidence object for this physical power-on epoch.
    """

    def __init__(self) -> None:
        self.rom_resets: list[tuple[int, str]] = []
        self.native_reset: int | None = None
        self.reset_observed: tuple[int, int] | None = None
        self.recovered: tuple[int, int, int] | None = None
        self.replay_verified = False
        self.halt_reason: str | None = None

    def observe(self, line: str) -> None:
        rom = ROM_RESET.search(line)
        if rom:
            self.rom_resets.append((int(rom.group(1), 16), rom.group(2)))
        boot = BOOT.search(line)
        if boot:
            if self.native_reset is not None:
                self.halt_reason = "MULTIPLE_POSTCUT_BOOT_EPOCHS"
            self.native_reset = int(boot.group(1))
        reset_details = RESET_OBSERVED.search(line)
        if reset_details:
            self.reset_observed = (
                int(reset_details.group(1)), int(reset_details.group(2))
            )
        if "EDP_BND|HALT|" in line:
            self.halt_reason = line.split("EDP_BND|HALT|", 1)[1].strip()
        recovered = RECOVER.search(line)
        if recovered:
            self.recovered = tuple(int(recovered.group(i)) for i in (1, 2, 3))
        if REPLAY_PASS in line:
            self.replay_verified = True

    def verdict(
        self,
        *,
        ack_before_cut: int,
        off_confirmed: bool,
        on_confirmed: bool,
        measured_off_seconds: float,
        minimum_off_seconds: float,
    ) -> CutVerdict:
        newest, older, count = self.recovered or (None, None, None)
        failure = ""
        if not off_confirmed or not on_confirmed:
            failure = "CONTROLLER_OFF_ON_NOT_VERIFIED"
        elif minimum_off_seconds < 0 or measured_off_seconds < minimum_off_seconds:
            failure = "OFF_DURATION_UNQUALIFIED"
        elif ack_before_cut < 2:
            failure = "INSUFFICIENT_PRECUT_BASELINE"
        elif self.halt_reason:
            failure = "FIRMWARE_HALT:" + self.halt_reason
        elif self.native_reset is None:
            failure = "POSTCUT_BOOT_NOT_OBSERVED"
        elif not self.rom_resets:
            failure = "POSTCUT_ROM_RESET_NOT_OBSERVED"
        elif self.reset_observed != (
            self.native_reset, int(self.native_reset == 1)
        ):
            failure = "APPLICATION_RESET_DIAGNOSTICS_INCONSISTENT"
        elif not self.replay_verified:
            failure = "PROTECTED_REPLAY_NOT_CONFIRMED"
        elif newest is None or older is None or count is None:
            failure = "RETAINED_WINDOW_NOT_OBSERVED"
        elif newest < ack_before_cut or older + 1 != newest or not (2 <= count <= 4):
            failure = "PROTECTED_RETAINED_WINDOW_FAILED"

        # POR is a narrower verified subtype; no other reset can impersonate it.
        has_rom_por = any(code == 1 and "POWERON" in label.upper()
                          for code, label in self.rom_resets)
        por = (self.native_reset == 1 and has_rom_por)
        classification = ("FAILED" if failure else
                          "POR_CONFIRMED" if por else
                          "EXTERNALLY_VERIFIED_POWER_CUT")
        return CutVerdict(
            accepted=not failure,
            classification=classification,
            failure=failure,
            native_reset=self.native_reset,
            rom_resets=tuple(self.rom_resets),
            recovered_newest=newest,
            recovered_older=older,
            recovered_count=count,
        )
