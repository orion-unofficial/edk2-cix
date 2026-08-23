#!/usr/bin/env python3

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import firmware_layout


class FirmwareLayoutTests(unittest.TestCase):
    def test_custom_release_leaf_path_nests_supported_features(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="RELEASE",
            enable_firmware_fixes=True,
            enable_core_order="performance",
            cix_release="1.2",
            enable_tf_a_fixes=True,
            enable_experimental_uefi_settings=True,
            uart3_enable=True,
            debug_verbose=True,
        )

        self.assertEqual(
            layout.leaf_path().as_posix(),
            "custom/cix/tf_a_fixes/fixes/core_order/performance/experimental/uart3/verbose",
        )

    def test_custom_debug_display_version_stays_within_budget(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="DEBUG",
            enable_firmware_fixes=True,
            enable_core_order="performance",
            cix_release="1.2",
            enable_tf_a_fixes=True,
            enable_experimental_uefi_settings=True,
            debug_on_uart3=True,
        )

        version = firmware_layout.display_version("1.2.1", layout, "abcdef1234")

        self.assertLessEqual(len(version), firmware_layout.MAX_DISPLAY_VERSION_LENGTH)
        self.assertIn("debug", version)
        self.assertIn("abcdef1", version)

    def test_archive_suffix_keeps_cix_before_debug(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="DEBUG",
            cix_release="v1.2",
            enable_firmware_fixes=True,
            enable_core_order="performance",
        )

        self.assertEqual(
            layout.archive_suffix(),
            "custom+cix+debug+fixes+core_order-performance",
        )

    def test_custom_release_baseline_reports_custom(self) -> None:
        layout = firmware_layout.FirmwareLayout(artefact_mode="custom", firmware_target="RELEASE")

        self.assertEqual(firmware_layout.display_version("1.2.1", layout), "1.2.1+custom")

    def test_iter_build_all_variants_has_expected_count(self) -> None:
        variants = firmware_layout.iter_build_all_variants()

        self.assertEqual(len(variants), 46)
        self.assertEqual(variants[0].artefact_mode, "upstream")
        self.assertEqual(variants[0].firmware_target, "RELEASE")
        leaf_paths = {variant.leaf_path().as_posix() for variant in variants}
        self.assertIn("custom/cix/debug/fixes/core_order/conventional", leaf_paths)
        self.assertIn("custom/cix/tf_a_fixes/debug/fixes/core_order/conventional", leaf_paths)
        self.assertIn("custom/cix/debug/experimental", leaf_paths)
        self.assertIn("custom/cix/debug/uart3/uart3_debug", leaf_paths)

    def test_validate_debian_version_uses_upstream_component(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir_text:
            repo_root = Path(tempdir_text)
            (repo_root / "debian").mkdir(parents=True)
            (repo_root / "VERSION").write_text("1.2.1\n", encoding="utf-8")
            (repo_root / "debian" / "changelog").write_text(
                "edk2-cix (1.2.1-7) main; urgency=medium\n",
                encoding="utf-8",
            )

            firmware_version, debian_version = firmware_layout.validate_debian_version(repo_root)

            self.assertEqual(firmware_version, "1.2.1")
            self.assertEqual(debian_version, "1.2.1-7")

    def test_firmware_build_paths_keep_debian_version_internal_to_upstream_compat(self) -> None:
        repo_root = SCRIPT_DIR.parent
        src_makefile = (repo_root / "src" / "Makefile").read_text(encoding="utf-8")
        radxa_define = (
            repo_root / "src" / "edk2-platforms" / "Platform" / "Radxa" / "RadxaDefine.dsc.inc"
        ).read_text(encoding="utf-8")
        radxa_common = (
            repo_root / "src" / "edk2-platforms" / "Platform" / "Radxa" / "RadxaCommon.dsc.inc"
        ).read_text(encoding="utf-8")
        wrapper_makefile = (repo_root / ".github" / "local" / "Makefile.local").read_text(
            encoding="utf-8"
        )

        self.assertIn('-D DEB_VERSION=$(FIRMWARE_VERSION)', src_makefile)
        self.assertIn('-D UEFI_FW_VERSION=$(UEFI_FW_VERSION)', src_makefile)
        self.assertIn('--cix-release "$(CIX_RELEASE_NORMALIZED)"', src_makefile)
        self.assertIn('--enable-tf-a-fixes TRUE', src_makefile)
        self.assertIn(".edk2-cix-build-config", src_makefile)
        self.assertIn('--cix-release "$(CIX_RELEASE_NORMALIZED)"', wrapper_makefile)
        self.assertIn('--enable-tf-a-fixes TRUE', wrapper_makefile)
        self.assertIn('EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE="$(EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE)"', wrapper_makefile)
        self.assertIn('EDK2_CIX_SUPPRESS_BUILD_BANNER="$(EDK2_CIX_SUPPRESS_BUILD_BANNER)"', wrapper_makefile)
        self.assertIn('ENABLE_TF_A_FIXES="$(ENABLE_TF_A_FIXES_NORMALIZED)"', wrapper_makefile)
        self.assertIn("FIRMWARE_VARIANT_OPTIONS_LABEL", wrapper_makefile)
        self.assertIn("[build] Building firmware: %s (%s, %s)", wrapper_makefile)
        self.assertIn('EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE ?= 0', src_makefile)
        self.assertNotIn('-D DEB_VERSION=$(DEB_VERSION)', src_makefile)
        self.assertNotIn('-D FIRMWARE_VERSION=$(FIRMWARE_VERSION)', src_makefile)
        self.assertIn("COMPILE_DEB_VERSION", radxa_define)
        self.assertIn("COMPILE_UEFI_FW_VERSION", radxa_define)
        self.assertIn("COMPILE_DEB_VERSION", radxa_common)
        self.assertIn("COMPILE_UEFI_FW_VERSION", radxa_common)
        self.assertNotIn("DEB_VERSION=\"$(DEB_VERSION)\"", wrapper_makefile)


if __name__ == "__main__":
    unittest.main()
