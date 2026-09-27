#!/usr/bin/env python3
"""Regression tests for clean-merge failures in custom Radxa 1.3 sources."""

from __future__ import annotations

from pathlib import Path
import unittest
from unittest.mock import patch

from validate_radxa13_source import (
    CPU_ASL, Entry, GitTree, O6, PCIE_MENU, SMBIOS, validate,
)


ROOT = Path(__file__).resolve().parents[1]


class MutationTree(GitTree):
    """Change one source blob/mode in memory, leaving retained refs untouched."""

    def __init__(self, source: GitTree, changes: dict[str, str],
                 modes: dict[str, str] | None = None):
        self.repo = source.repo
        self.revision = source.revision
        self.entries = source.entries.copy()
        self.changes = changes
        for path, mode in (modes or {}).items():
            entry = self.entries[path]
            self.entries[path] = Entry(mode, entry.oid)

    def text(self, path: str) -> str:
        return self.changes.get(path, super().text(path))


def mutated_problems(source: GitTree, edk2: str, radxa: str,
                     changes: dict[str, str], modes: dict[str, str] | None = None) -> list[str]:
    tree = MutationTree(source, changes, modes)
    with patch("validate_radxa13_source.GitTree", return_value=tree):
        return validate(ROOT, "in-memory-mutation", edk2, radxa)


class Radxa13StructuralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.reference = GitTree(ROOT, "source/unofficial/1.3.1/edk2-stable202208")

    def test_retained_positive_sources_pass(self) -> None:
        for edk2, radxa in (("202208", "1.3.1"), ("202605", "1.3.0"),
                            ("202605", "1.3.1"), ("202608", "1.3.1")):
            with self.subTest(edk2=edk2, radxa=radxa):
                revision = f"source/unofficial/{radxa}/edk2-stable{edk2}"
                self.assertEqual([], validate(ROOT, revision, edk2, radxa))

    def test_202208_130_profile_matches_retained_202208_shape(self) -> None:
        # The fifth positive is a private 1.3.0 candidate, not a retained main
        # ref.  The retained 1.3.1 source checks this profile's shared layout;
        # the private candidate is checked separately by the caller's CLI.
        self.assertEqual([], validate(
            ROOT, "source/unofficial/1.3.1/edk2-stable202208", "202208", "1.3.0"))

    def test_old_pilot_clean_merge_failures_are_caught(self) -> None:
        source = self.reference
        smbios = f"src/{SMBIOS}/PlatformSmbios.c"
        # The pilot commit itself lived in an earlier private repository and
        # is not a durable ref here.  Recreate its documented bad semantics
        # against the retained positive tree without writing Git objects.
        cpu = source.text(CPU_ASL).replace("CORE_0_TO_3_DESIRED_PERF_REG", "CORE_4_5_DESIRED_PERF_REG", 2)
        fdf = f"custom/overlay/edk2-platforms/{O6}/O6.fdf"
        experimental = f"custom/overlay-experimental-uefi-settings/edk2-platforms/{O6}/O6.fdf"
        menu = f"custom/overlay-experimental-uefi-settings/{PCIE_MENU}/PcieConfig.hfr"
        make = source.text("src/Makefile").replace("PCIE_SMMU_IORT_VALIDATOR", "LOST_IORT_CHECK")
        make = make.replace("-DENABLE_FIRMWARE_FIXES=1'", "'", 2)
        changes = {
            smbios: source.text(smbios).replace("PLATFORM_SMBIOS_TABLE_HOOK", "SmbiosType4, SmbiosType7"),
            CPU_ASL: cpu,
            fdf: source.text(fdf).replace("0x001f4000", "0x00200000").replace("0x1f4", "0x200") + "\n# Radxa 1.2.4\n",
            experimental: "../../../../../../overlay/edk2-platforms/Platform/Radxa/Orion/O6/missing.fdf",
            "src/Makefile": make,
            menu: source.text(menu).replace("RadxaSetupVar.PcieDeviceModel", "RadxaSetupVar.OldPcieSetting"),
        }
        problems = mutated_problems(source, "202208", "1.3.1", changes)
        for fragment in (
            "removed SMBIOS Type 4/7", "CPU0 has wrong CPPC", "CPU1 has wrong CPPC",
            "wrong DEBUG/RELEASE BL33", "stale 1.2.4", "overlay symlink target is missing",
            "missing PCIe SMMU IORT validator hook", "missing gated PCIe device-model UI",
        ):
            with self.subTest(fragment=fragment):
                self.assertTrue(any(fragment in problem for problem in problems), problems)

    def test_symlink_mode_must_be_preserved(self) -> None:
        path = f"custom/overlay/{SMBIOS}/PlatformSmbios.c"
        problems = mutated_problems(self.reference, "202208", "1.3.1", {}, {path: "100644"})
        self.assertTrue(any(f"{path}: expected Git mode 120000" in problem for problem in problems), problems)

    def test_cpu4_and_cpu5_cannot_use_little_core_cppc_registers(self) -> None:
        data = self.reference.text(CPU_ASL).replace(
            "CORE_4_5_DESIRED_PERF_REG", "CORE_0_TO_3_DESIRED_PERF_REG", 2)
        problems = mutated_problems(self.reference, "202208", "1.3.1", {CPU_ASL: data})
        for cpu in (4, 5):
            self.assertTrue(any(f"CPU{cpu} has wrong CPPC register group" in problem
                                for problem in problems), problems)

    def test_202605_experimental_fdf_must_keep_its_own_layout(self) -> None:
        source = GitTree(ROOT, "source/unofficial/1.3.0/edk2-stable202605")
        fdf = f"custom/overlay-experimental-uefi-settings/edk2-platforms/{O6}/O6.fdf"
        data = source.text(fdf).replace("0x001f2000", "0x00200000")
        size_problems = mutated_problems(source, "202605", "1.3.0", {fdf: data})
        mode_problems = mutated_problems(source, "202605", "1.3.0", {}, {fdf: "120000"})
        self.assertTrue(any("expected regular experimental FDF" in problem for problem in mode_problems))
        self.assertTrue(any("wrong DEBUG/RELEASE BL33" in problem for problem in size_problems))

    def test_unqualified_pair_fails_closed(self) -> None:
        self.assertIn("unsupported structural profile", validate(ROOT, "HEAD", "202211", "1.3.0")[0])


if __name__ == "__main__":
    unittest.main()
