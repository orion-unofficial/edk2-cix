#!/usr/bin/env python3

import hashlib
import os
import subprocess
import sys
from pathlib import Path
import unittest
from unittest.mock import patch

from validate_build_variables import validate_cix_source, validate_feature_relationships
from reconstruction_common import ReconstructionError, active_unofficial_source_ref, release_entry, show_file, unofficial_source_ref


REPO_ROOT = Path(__file__).resolve().parents[1]


class ValidateBuildVariableTests(unittest.TestCase):
    def test_retired_conventional_order_fails_before_source_resolution(self) -> None:
        result = subprocess.run([sys.executable, str(REPO_ROOT / 'scripts/validate_build_variables.py')],
                                env={**os.environ, 'ENABLE_CORE_ORDER': 'conventional'},
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('ENABLE_CORE_ORDER', result.stderr)
        self.assertIn('cix, performance', result.stderr)

    def test_hybrid_cannot_select_vendor_optee(self) -> None:
        with patch.dict(os.environ, {"CIX_RELEASE": "1.2", "STMM": "unofficial"}, clear=True):
            problems: list[str] = []
            validate_cix_source(REPO_ROOT, {"source_ref": "source/unofficial/1.3/current"}, problems)
            self.assertEqual(len(problems), 1)
            self.assertIn("STMM=unofficial requires CIX_RELEASE=unofficial", problems[0])

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
        for radxa in ("1.2.4", "1.3.1"):
            for edk2 in ("202208", "202605", "202608"):
                with self.subTest(radxa=radxa, edk2=edk2):
                    supported: list[str] = []
                    validate_cix_source(
                        REPO_ROOT,
                        {"source_ref": active_unofficial_source_ref(REPO_ROOT, radxa, f"edk2-stable{edk2}")
                         or unofficial_source_ref(REPO_ROOT, radxa, f"edk2-stable{edk2}")},
                        supported,
                    )
                    self.assertEqual(supported, [])
        unsupported: list[str] = []
        validate_cix_source(
            REPO_ROOT, {"source_ref": "source/unofficial/1.2.1/edk2-stable202208"},
            unsupported)
        self.assertIn("CIX_RELEASE is unavailable", unsupported[0])

    def test_native_profile_binds_all_helpers_and_release(self) -> None:
        from validate_build_variables import CIX_NATIVE_SIGNING_HELPER_SHA256, CIX_STMM_HELPER_PROFILES

        signer_path = "src/scripts/build_cix_release_bootloader2.sh"
        helpers = CIX_STMM_HELPER_PROFILES[CIX_NATIVE_SIGNING_HELPER_SHA256]
        self.assertEqual(len(helpers), 4)
        ref = "source/unofficial/1.3.1/edk2-stable202608"
        entry = {
            "source_ref": ref,
            "edk2_release": "edk2-stable202608",
            "radxa_release": "1.3.1",
            "render": {"steps": [{"component": {"path": path}} for path in (
                "src/cix-v1.2/tee-unofficial", "src/cix-v1.2/stmm-unofficial")]},
        }
        signer = b"native signer fixture"
        signer_hash = hashlib.sha256(signer).hexdigest()
        companions = {path: path.encode() for path in helpers}
        profile = {path: hashlib.sha256(data).hexdigest() for path, data in companions.items()}
        files = {signer_path: signer, **companions}
        from validate_firmware_chain import CIX_KEY_SHA256, CIX_SIGNING_KEYS
        files.update({f"{CIX_SIGNING_KEYS}/{name}": show_file(REPO_ROOT, ref, f"{CIX_SIGNING_KEYS}/{name}")
                      for name in CIX_KEY_SHA256})

        def read_file(repo, ref, path, **kwargs):
            if path == "src/Makefile":
                return b"tee-unofficial CIX_RELEASE_SIGNING_KEYS_DIR"
            if path.endswith("/ArmFfaBootInfo.h"):
                return b"FFA boot info"
            return files.get(path, b"")

        def validate(selection):
            problems = []
            validate_cix_source(REPO_ROOT, selection, problems)
            return problems

        with patch.dict(os.environ, {"STMM": "unofficial", "CIX_RELEASE": "unofficial"}, clear=True), \
                patch("stmm_integration.selected", return_value=False), \
                patch("validate_build_variables.CIX_NATIVE_SIGNING_HELPER_SHA256", signer_hash), \
                patch.dict(CIX_STMM_HELPER_PROFILES, {signer_hash: profile}), \
                patch("validate_build_variables.show_file", side_effect=read_file):
            self.assertEqual(validate(entry), [])
            self.assertIn("requires EDK2 202608", validate({**entry, "edk2_release": "edk2-stable202605"})[0])
            self.assertIn("Radxa 1.3.1", validate({**entry, "radxa_release": "1.2.4"})[0])
            self.assertIn("unofficial-checkpoint", validate(
                {**entry, "source_ref": "source/unofficial/1.3/current"})[0])
            with patch.dict(os.environ, {"FIRMWARE_TARGET": "DEBUG"}):
                self.assertIn("O6 RELEASE", validate(entry)[0])
            for path in companions:
                with self.subTest(path=path):
                    for changed in (b"", companions[path] + b" changed"):
                        with patch.dict(files, {path: changed}):
                            self.assertIn(path, validate(entry)[0])
            with patch.dict(files, {signer_path: signer + b" changed"}):
                self.assertIn("signing helper", validate(entry)[0])
            self.assertIn("component is not selected", validate(
                {**entry, "render": {"steps": entry["render"]["steps"][:1]}})[0])

    def test_source_without_reviewed_stmm_is_not_advertised(self) -> None:
        source = "source/unofficial/1.3.1/edk2-stable202608"
        for missing in (False, True):
            def staged_file(repo, ref, path):
                if path == "src/scripts/stage_vendor_stmm.py":
                    if missing:
                        raise ReconstructionError("StandaloneMM staging helper is missing")
                    return b"unreviewed staging helper"
                return show_file(repo, ref, path)

            with self.subTest(missing=missing), patch("validate_build_variables.show_file", side_effect=staged_file):
                problems: list[str] = []
                validate_cix_source(REPO_ROOT, {"source_ref": source}, problems)
                self.assertEqual(len(problems), 1)
                self.assertIn("StandaloneMM", problems[0])
        supported: list[str] = []
        validate_cix_source(REPO_ROOT, {"source_ref": source}, supported)
        self.assertEqual(supported, [])

    def test_unofficial_requires_rendered_component_on_supported_checkpoints(self) -> None:
        with patch.dict(os.environ, {"CIX_RELEASE": "unofficial"}, clear=True):
            for radxa in ("1.2.4", "1.3.1"):
                for edk2 in ("202208", "202605", "202608"):
                    with self.subTest(radxa=radxa, edk2=edk2):
                        _, entry = release_entry(
                            REPO_ROOT, f"source/cache/release/custom/edk2-{edk2}/cix-1.2/radxa-{radxa}/unofficial")
                        problems: list[str] = []
                        validate_cix_source(REPO_ROOT, entry, problems)
                        self.assertEqual(problems, [])
                        validate_cix_source(REPO_ROOT, {"source_ref": entry["source_ref"]}, problems)
                        self.assertEqual(len(problems), 1)
                        self.assertIn("component is not selected", problems[0])


if __name__ == "__main__":
    unittest.main()
