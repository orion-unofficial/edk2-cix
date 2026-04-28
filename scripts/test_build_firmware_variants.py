#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import io
import sys
import unittest
from unittest import mock
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import build_firmware_variants
import firmware_layout


class BuildFirmwareVariantsTests(unittest.TestCase):
    def test_default_distro_follows_artefact_mode(self) -> None:
        upstream = firmware_layout.FirmwareLayout(
            artefact_mode="upstream",
            firmware_target="RELEASE",
        )
        custom = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="RELEASE",
        )

        self.assertEqual(build_firmware_variants.default_distro_for_layout(upstream), "bookworm")
        self.assertEqual(build_firmware_variants.default_distro_for_layout(custom), "trixie")

    def test_variant_label_matches_leaf_path(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="DEBUG",
            cix_release="v1.2",
            enable_firmware_fixes=True,
        )

        self.assertEqual(
            build_firmware_variants.variant_label(layout),
            "custom/cix/debug/fixes",
        )

    def test_upstream_variant_label_is_vendor(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="upstream",
            firmware_target="RELEASE",
        )

        self.assertEqual(build_firmware_variants.variant_label(layout), "vendor")

    def test_custom_variants_enable_incremental_workspace_reuse(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="RELEASE",
        )

        args = build_firmware_variants.variant_make_args(
            layout,
            board="O6",
            product="orion-o6",
            version="1.2.1",
            stage_root=Path("/tmp/stage"),
            distro="trixie",
        )

        self.assertIn("EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE=1", args)
        self.assertIn("EDK2_CIX_SUPPRESS_BUILD_BANNER=1", args)
        self.assertIn("ENABLE_FIRMWARE_FIXES=", args)
        self.assertIn("ENABLE_CORE_ORDER=", args)
        self.assertIn("CIX_RELEASE=", args)
        self.assertIn("ENABLE_EXPERIMENTAL_UEFI_SETTINGS=", args)
        self.assertIn("DEBUG_ON_UART3=", args)
        self.assertIn("UART3_ENABLE=", args)
        self.assertIn("DEBUG_VERBOSE=", args)
        self.assertIn("DEBUG_PRINT_ERROR_LEVEL=", args)

    def test_global_debug_print_error_level_applies_only_to_custom_variants(self) -> None:
        custom = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="RELEASE",
            enable_firmware_fixes=True,
        )
        upstream = firmware_layout.FirmwareLayout(
            artefact_mode="upstream",
            firmware_target="RELEASE",
        )

        custom_args = build_firmware_variants.variant_make_args(
            custom,
            board="O6",
            product="orion-o6",
            version="1.2.1",
            stage_root=Path("/tmp/custom"),
            distro="trixie",
            debug_print_error_level="0x80000040",
        )
        upstream_args = build_firmware_variants.variant_make_args(
            upstream,
            board="O6",
            product="orion-o6",
            version="1.2.1",
            stage_root=Path("/tmp/upstream"),
            distro="bookworm",
            debug_print_error_level="0x80000040",
        )

        self.assertIn("DEBUG_PRINT_ERROR_LEVEL=0x80000040", custom_args)
        self.assertIn("DEBUG_PRINT_ERROR_LEVEL=", upstream_args)
        self.assertNotIn("DEBUG_PRINT_ERROR_LEVEL=0x80000040", upstream_args)

    def test_ccache_arguments_are_available_to_upstream_and_custom_variants(self) -> None:
        custom = firmware_layout.FirmwareLayout(
            artefact_mode="custom",
            firmware_target="RELEASE",
        )
        upstream = firmware_layout.FirmwareLayout(
            artefact_mode="upstream",
            firmware_target="RELEASE",
        )

        common_kwargs = {
            "board": "O6",
            "product": "orion-o6",
            "version": "1.2.1",
            "stage_root": Path("/tmp/stage"),
            "distro": "bookworm",
            "ccache_dir": "/tmp/ccache",
            "ccache_wrapper_root": "/tmp/wrappers",
        }

        for layout in (custom, upstream):
            with self.subTest(artefact_mode=layout.artefact_mode):
                args = build_firmware_variants.variant_make_args(layout, **common_kwargs)
                self.assertIn("CCACHE_DIR=/tmp/ccache", args)
                self.assertIn("CCACHE_WRAPPER_ROOT=/tmp/wrappers", args)

    def test_first_upstream_variant_reports_ccache_stats(self) -> None:
        layout = firmware_layout.FirmwareLayout(
            artefact_mode="upstream",
            firmware_target="RELEASE",
        )
        previous_stats = {
            "direct_cache_hit": 1,
            "preprocessed_cache_hit": 0,
            "cache_miss": 2,
            "cache_size_kibibyte": 1024,
        }
        next_stats = {
            "direct_cache_hit": 4,
            "preprocessed_cache_hit": 1,
            "cache_miss": 4,
            "cache_size_kibibyte": 2048,
        }

        output = io.StringIO()
        with (
            mock.patch("build_firmware_variants.subprocess.run") as run,
            mock.patch("build_firmware_variants.fetch_ccache_stats", return_value=next_stats),
            contextlib.redirect_stdout(output),
        ):
            returned_stats = build_firmware_variants.build_variant(
                repo_root=Path("/repo"),
                make_executable="make",
                verbosity="0",
                board="O6",
                product="orion-o6",
                version="1.2.1",
                layout=layout,
                variant_root=Path("/tmp/variants"),
                debug_print_error_level="",
                index=1,
                total=1,
                labels=["vendor"],
                previous_ccache_stats=previous_stats,
                build_all_started_at=0.0,
                ccache_dir="/tmp/ccache",
                ccache_wrapper_root="/tmp/wrappers",
                ccache_bin="ccache",
                ccache_disable=None,
            )

        self.assertEqual(returned_stats, next_stats)
        self.assertIn(
            "[build-all] Building variant 1/1: vendor (bookworm, vendor)",
            output.getvalue(),
        )
        self.assertIn("[build-all] Completed variant 1/1: vendor", output.getvalue())
        self.assertIn(
            "[build-all] ccache: 6 cacheable, 4 hits (3 direct, 1 preprocessed), "
            "2 misses, 66.7% hit rate, +1.0 MiB",
            output.getvalue(),
        )
        command = run.call_args.args[0]
        self.assertIn("buildbox-firmware-stage", command)
        self.assertIn("ARTEFACT_MODE=upstream", command)
        self.assertIn("EDK2_CIX_SUPPRESS_BUILD_BANNER=1", command)
        self.assertIn("CCACHE_DIR=/tmp/ccache", command)
        self.assertFalse(any(arg.startswith("ENABLE_" + "CCACHE=") for arg in command))

    def test_traversal_groups_keep_release_before_debug(self) -> None:
        variants = firmware_layout.iter_build_all_variants()
        phases: list[str] = []

        for layout in variants:
            label = build_firmware_variants.traversal_group_label(layout)
            if not phases or phases[-1] != label:
                phases.append(label)

        self.assertEqual(phases, ["vendor", "custom", "custom/debug"])

    def test_release_phase_interleaves_cix_inside_each_compile_bucket(self) -> None:
        variants = firmware_layout.iter_build_all_variants()
        labels = [variant.archive_suffix() or "upstream" for variant in variants[1:17]]

        self.assertEqual(
            labels,
            [
                "custom",
                "custom+cix",
                "custom+fixes",
                "custom+cix+fixes",
                "custom+fixes+core_order-conventional",
                "custom+cix+fixes+core_order-conventional",
                "custom+fixes+core_order-performance",
                "custom+cix+fixes+core_order-performance",
                "custom+experimental",
                "custom+cix+experimental",
                "custom+verbose",
                "custom+cix+verbose",
                "custom+uart3",
                "custom+cix+uart3",
                "custom+uart3+uart3_debug",
                "custom+cix+uart3+uart3_debug",
            ],
        )

    def test_format_duration_prefers_readable_units(self) -> None:
        self.assertEqual(build_firmware_variants.format_duration(5), "5s")
        self.assertEqual(build_firmware_variants.format_duration(65), "1m 5s")
        self.assertEqual(build_firmware_variants.format_duration(3665), "1h 1m 5s")

    def test_summarize_ccache_delta_reports_hits_misses_and_growth(self) -> None:
        summary = build_firmware_variants.summarize_ccache_delta(
            before={
                "direct_cache_hit": 10,
                "preprocessed_cache_hit": 2,
                "cache_miss": 8,
                "cache_size_kibibyte": 1024,
            },
            after={
                "direct_cache_hit": 25,
                "preprocessed_cache_hit": 4,
                "cache_miss": 11,
                "cache_size_kibibyte": 3072,
            },
        )

        self.assertEqual(
            summary,
            "20 cacheable, 17 hits (15 direct, 2 preprocessed), 3 misses, 85.0% hit rate, +2.0 MiB",
        )


if __name__ == "__main__":
    unittest.main()
