#!/usr/bin/env python3
"""Pure host negatives for independent physical LittleFS extraction auditing."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from audit_lowspace_partition import inspect_extracted_partition


class LowSpacePartitionAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self.work = TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        (self.root / "seed.txt").write_bytes(b"EDP1")
        (self.root / "g0002.bin").write_bytes(bytes([2]) * 8192)
        (self.root / "g0003.bin").write_bytes(bytes([3]) * 8192)

    def test_complete_rolled_window(self):
        self.assertEqual(
            inspect_extracted_partition(self.root)["protected_newer"], 3
        )

    def test_still_retained_old_generation(self):
        (self.root / "g0001.bin").write_bytes(bytes([1]) * 8192)
        with self.assertRaisesRegex(ValueError, "unexpected retained"):
            inspect_extracted_partition(self.root)

    def test_unremoved_unprotected_filler(self):
        (self.root / "pad000.bin").write_bytes(b"padding")
        with self.assertRaisesRegex(ValueError, "unexpected retained"):
            inspect_extracted_partition(self.root)

    def test_truncated_protected_bytes(self):
        (self.root / "g0003.bin").write_bytes(bytes([3]) * 4096)
        with self.assertRaisesRegex(ValueError, "short or oversized"):
            inspect_extracted_partition(self.root)

    def test_corrupted_middle_byte(self):
        target = bytearray((self.root / "g0002.bin").read_bytes())
        target[4072] ^= 1
        (self.root / "g0002.bin").write_bytes(target)
        with self.assertRaisesRegex(ValueError, "corruption"):
            inspect_extracted_partition(self.root)

    def test_missing_successor(self):
        (self.root / "g0003.bin").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            inspect_extracted_partition(self.root)

    def test_incorrect_seed(self):
        (self.root / "seed.txt").write_bytes(b"NOPE")
        with self.assertRaisesRegex(ValueError, "seed invalid"):
            inspect_extracted_partition(self.root)

    def test_directory_disguised_as_record(self):
        (self.root / "g0003.bin").unlink()
        (self.root / "g0003.bin").mkdir()
        with self.assertRaisesRegex(ValueError, "invalid protected"):
            inspect_extracted_partition(self.root)


if __name__ == "__main__":
    unittest.main()
