#!/usr/bin/env python3

import os
from pathlib import Path
import unittest
from unittest.mock import patch

from validate_build_variables import validate_feature_relationships


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


if __name__ == "__main__":
    unittest.main()
