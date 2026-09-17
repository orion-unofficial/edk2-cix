#!/usr/bin/env python3
"""Check that compilation coverage cannot silently drop valid release targets."""

import json
import unittest

from firmware_ci_matrix import CUSTOM_TARGET, ROOT, firmware_matrix
from reconstruction_common import matrix_release_branches, source_target_name


class FirmwareCIMatrixTests(unittest.TestCase):
    def test_every_completion_has_both_boards_and_fix_states(self) -> None:
        targets = [
            f"edk2-{edk2}/radxa-{radxa}/unofficial"
            for edk2 in ("202208", "202605", "202608", "202611")
            for radxa in ("1.2.4", "1.3.1")
        ]
        rows = firmware_matrix(targets + [targets[0], "edk2-202608/cix-1.2/radxa-1.3.1/unofficial"], ["1.2.4", "1.3.1"])["include"]
        self.assertEqual(len(rows), len(targets) * 4)
        self.assertEqual(len({row["key"] for row in rows}), len(rows))
        for target in targets:
            self.assertEqual(
                {(row["board"], row["firmware_fixes"]) for row in rows if row["release"] == target},
                {("O6", False), ("O6", True), ("O6N", False), ("O6N", True)},
            )

    def test_missing_required_release_is_an_error(self) -> None:
        with self.assertRaisesRegex(ValueError, "Radxa 1.3.1"):
            firmware_matrix(["edk2-202208/radxa-1.2.4/unofficial"], ["1.2.4", "1.3.1"])

    def test_matrix_capacity_never_silently_drops_releases(self) -> None:
        targets = [f"edk2-{release}/radxa-{radxa}/unofficial"
                   for release in range(33) for radxa in ("1.2.4", "1.3.1")]
        with self.assertRaisesRegex(ValueError, "shard it without dropping source targets"):
            firmware_matrix(targets, ["1.2.4", "1.3.1"])

    def test_real_matrix_covers_all_public_completions(self) -> None:
        policy = json.loads((ROOT / "config/policies.json").read_text())
        required = policy["firmware_qualification_policy"]["radxa_releases"]
        self.assertTrue({"1.2.4", "1.3.1"}.issubset(required))
        branches, _ = matrix_release_branches(ROOT)
        targets = [source_target_name(branch) for branch in branches]
        expected = {target for target in targets if (match := CUSTOM_TARGET.fullmatch(target)) and match["radxa"] in required}
        matrix = firmware_matrix(targets, required)
        self.assertEqual({row["release"] for row in matrix["include"]}, expected)
        self.assertEqual(len(matrix["include"]), len(expected) * 4)


if __name__ == "__main__":
    unittest.main()
