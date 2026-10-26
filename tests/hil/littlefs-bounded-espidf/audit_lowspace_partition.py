#!/usr/bin/env python3
"""Independent, read-only audit of the extracted low-space physical partition.

This test-only checker never mounts or repairs flash. The input directory
must be extracted from a separate read-flash of the 0x170000/0x90000
partition after a verified external-VDD cut and *completed postboot roll*.
It tests synthetic byte-pattern records, NOT production EDLG/EDLS CRC.
"""
from __future__ import annotations

from pathlib import Path
import sys


EXPECTED_NAMES = frozenset({"seed.txt", "g0002.bin", "g0003.bin"})
RECORD_BYTES = 8192


def inspect_extracted_partition(root: Path) -> dict[str, int]:
    """Fail closed if retired/filler/unknown paths or incorrect bytes remain."""
    if not root.is_dir():
        raise ValueError("extraction directory missing")
    names = {entry.name for entry in root.iterdir()}
    if names != EXPECTED_NAMES:
        raise ValueError(
            f"unexpected retained/unknown files: missing="
            f"{sorted(EXPECTED_NAMES - names)} extra="
            f"{sorted(names - EXPECTED_NAMES)}"
        )
    if (root / "seed.txt").read_bytes() != b"EDP1":
        raise ValueError("test partition provisioning seed invalid")

    for generation in (2, 3):
        name = f"g{generation:04d}.bin"
        file_path = root / name
        if not file_path.is_file() or file_path.is_symlink():
            raise ValueError(f"invalid protected record kind: {name}")
        data = file_path.read_bytes()
        if len(data) != RECORD_BYTES:
            raise ValueError(f"short or oversized protected record: {name}")
        if data != bytes([generation]) * RECORD_BYTES:
            raise ValueError(f"protected record corruption: {name}")

    return {"protected_older": 2, "protected_newer": 3,
            "retired_oldest": 1, "record_bytes": RECORD_BYTES}


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: audit_lowspace_partition.py <extracted-dir>")
    try:
        report = inspect_extracted_partition(Path(sys.argv[1]))
    except (ValueError, OSError) as exc:
        print(f"LOWSPACE_OFFLINE_AUDIT=FAIL reason={exc!s}", flush=True)
        raise SystemExit(1)
    print("LOWSPACE_OFFLINE_AUDIT=PASS " +
          " ".join(f"{k}={v}" for k, v in report.items()), flush=True)
