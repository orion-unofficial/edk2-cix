#!/usr/bin/env python3

import os
from pathlib import Path
import unittest
from unittest.mock import patch

from validate_build_variables import validate_cix_source, validate_feature_relationships


REPO_ROOT = Path(__file__).resolve().parents[1]


class ValidateBuildVariableTests(unittest.TestCase):
    def problems_for(self, **values: str) -> list[str]:
        with patch.dict(os.environ, values, clear=True):
            problems: list[str] = []
            validate_feature_relationships(REPO_ROOT, problems)
            return problems

    def test_upstream_mode_accepts_explicit_false_custom_booleans(self) -> None:
        problems = self.problems_for(
            ARTEFACT_MODE="upstream",
            ENABLE_FIRMWARE_FIXES="false",
            ENABLE_EXPERIMENTAL_UEFI_SETTINGS="0",
            DEBUG_ON_UART3="off",
            UART3_ENABLE="no",
            DEBUG_VERBOSE="false",
        )

        self.assertEqual(problems, [])

    def test_large_image_consent_is_exclusive_to_custom_builds(self) -> None:
        self.assertEqual(self.problems_for(ARTEFACT_MODE="custom", DEBUG_ALLOW_LARGE_IMAGE="1"), [])
        self.assertEqual(self.problems_for(ARTEFACT_MODE="upstream", DEBUG_ALLOW_LARGE_IMAGE="0"), [])
        self.assertEqual(len(self.problems_for(ARTEFACT_MODE="upstream", DEBUG_ALLOW_LARGE_IMAGE="1")), 1)

    def test_tf_a_fixes_require_a_source_built_cix_release(self) -> None:
        self.assertEqual(self.problems_for(ARTEFACT_MODE="custom", ENABLE_TF_A_FIXES="true",
                                           CIX_RELEASE="1.2"), [])
        self.assertIn("requires CIX_RELEASE=1.2",
                      self.problems_for(ARTEFACT_MODE="custom", ENABLE_TF_A_FIXES="true")[0])

    def test_source_built_cix_release_is_o6_only(self) -> None:
        self.assertEqual(self.problems_for(ARTEFACT_MODE="custom", CIX_RELEASE="1.2",
                                           FIRMWARE_BOARD="O6"), [])
        self.assertIn("only FIRMWARE_BOARD=O6",
                      self.problems_for(ARTEFACT_MODE="custom", CIX_RELEASE="1.2",
                                        FIRMWARE_BOARD="O6N")[0])

    def test_upstream_mode_rejects_enabled_or_valued_custom_options(self) -> None:
        problems = self.problems_for(
            ARTEFACT_MODE="upstream",
            ENABLE_FIRMWARE_FIXES="true",
            ENABLE_CORE_ORDER="cix",
            DEBUG_PRINT_ERROR_LEVEL="0",
            CIX_RELEASE="v1.2",
        )

        self.assertEqual(len(problems), 4)
        self.assertTrue(all("only supported with ARTEFACT_MODE=custom" in item for item in problems))

    def test_cix_release_requires_a_source_checkpoint_with_pinned_keys(self) -> None:
        supported: list[str] = []
        validate_cix_source(
            REPO_ROOT, {"source_ref": "source/unofficial/1.3.1/edk2-stable202608"},
            supported)
        self.assertEqual(supported, [])
        unsupported: list[str] = []
        validate_cix_source(
            REPO_ROOT, {"source_ref": "source/unofficial/1.2.1/edk2-stable202208"},
            unsupported)
        self.assertIn("CIX_RELEASE=1.2 is unavailable", unsupported[0])


if __name__ == "__main__":
    unittest.main()
