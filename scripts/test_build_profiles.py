#!/usr/bin/env python3

from pathlib import Path
import unittest

from build_profiles import resolve_profile
from reconstruction_common import ReconstructionError


REPO_ROOT = Path(__file__).resolve().parents[1]


class BuildProfileTests(unittest.TestCase):
    def test_default_profile_is_exact_current_upstream_replay(self) -> None:
        profile = resolve_profile(REPO_ROOT)

        self.assertEqual(profile["profile"], "upstream")
        self.assertEqual(profile["build_kind"], "deterministic-replay")
        self.assertEqual(profile["release"], "edk2-202208/radxa-1.3.1")
        self.assertEqual(profile["replay_version"], "1.3.1")
        self.assertEqual(profile["artefact_mode"], "upstream")
        self.assertEqual(profile["enable_firmware_fixes"], "false")
        self.assertEqual(profile["cix_early_boot_release"], "")

    def test_latest_profile_uses_maintained_source_without_fixes(self) -> None:
        profile = resolve_profile(REPO_ROOT, requested_profile="latest")

        self.assertEqual(profile["build_kind"], "source-build")
        self.assertIn("edk2-202608/cix-1.2/radxa-1.3.1/unofficial", profile["release"])
        self.assertEqual(profile["artefact_mode"], "custom")
        self.assertEqual(profile["enable_firmware_fixes"], "false")
        self.assertEqual(profile["cix_early_boot_release"], "1.2")

    def test_latest_profile_allows_explicit_fixes(self) -> None:
        profile = resolve_profile(
            REPO_ROOT,
            requested_profile="latest",
            firmware_fixes_override="true",
        )

        self.assertEqual(profile["enable_firmware_fixes"], "true")

    def test_upstream_profile_accepts_explicit_false_but_rejects_active_custom_options(self) -> None:
        profile = resolve_profile(
            REPO_ROOT,
            firmware_fixes_override="false",
            custom_options=["DEBUG_ON_UART3=false"],
        )
        self.assertEqual(profile["enable_firmware_fixes"], "false")

        with self.assertRaisesRegex(ReconstructionError, "ENABLE_FIRMWARE_FIXES=true"):
            resolve_profile(REPO_ROOT, firmware_fixes_override="true")
        with self.assertRaisesRegex(ReconstructionError, "DEBUG_ON_UART3"):
            resolve_profile(REPO_ROOT, custom_options=["DEBUG_ON_UART3=true"])

    def test_profile_rejects_semantically_conflicting_low_level_overrides(self) -> None:
        with self.assertRaisesRegex(ReconstructionError, "requires ARTEFACT_MODE=upstream"):
            resolve_profile(REPO_ROOT, artefact_mode_override="custom")
        with self.assertRaisesRegex(ReconstructionError, "requires FIRMWARE_TARGET=RELEASE"):
            resolve_profile(REPO_ROOT, firmware_target="DEBUG")
        with self.assertRaisesRegex(ReconstructionError, "fixes RELEASE"):
            resolve_profile(REPO_ROOT, release_override="some/other/target")


if __name__ == "__main__":
    unittest.main()
