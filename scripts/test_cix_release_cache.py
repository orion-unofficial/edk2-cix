#!/usr/bin/env python3

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
import shlex

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import cix_release_cache


class CixReleaseCacheTests(unittest.TestCase):
    def test_tee_tree_fingerprint_ignores_generated_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir_text:
            tee_dir = Path(tmpdir_text) / "tee"
            tee_dir.mkdir()
            (tee_dir / "Makefile").write_text("all:\n\t@true\n", encoding="utf-8")
            source = tee_dir / "core"
            source.mkdir()
            (source / "main.c").write_text("int main(void) { return 0; }\n", encoding="utf-8")

            baseline = cix_release_cache.tree_fingerprint(
                tee_dir,
                excluded_prefixes=("out", ".git", "__pycache__"),
                excluded_exact=("tee.bin",),
                excluded_suffixes=(".pyc",),
            )

            out_dir = tee_dir / "out" / "arm-plat-cix" / "core"
            out_dir.mkdir(parents=True)
            (out_dir / "tee-raw.bin").write_bytes(b"generated")
            (tee_dir / "tee.bin").write_bytes(b"generated wrapper")

            self.assertEqual(
                baseline,
                cix_release_cache.tree_fingerprint(
                    tee_dir,
                    excluded_prefixes=("out", ".git", "__pycache__"),
                    excluded_exact=("tee.bin",),
                    excluded_suffixes=(".pyc",),
                ),
            )

    def test_cache_plan_changes_when_stmm_payload_changes(self) -> None:
        plan_a = cix_release_cache.build_cache_plan(
            cert_create_tree_fingerprint="cert-tree",
            tfa_tree_fingerprint="tfa-tree",
            tee_tree_fingerprint="tee-tree",
            helper_fingerprint="helper",
            host_compiler_fingerprint="host-cc",
            cross_compiler_fingerprint="cross-cc",
            mode="release",
            stmm_fingerprint="aaa",
        )
        plan_b = cix_release_cache.build_cache_plan(
            cert_create_tree_fingerprint="cert-tree",
            tfa_tree_fingerprint="tfa-tree",
            tee_tree_fingerprint="tee-tree",
            helper_fingerprint="helper",
            host_compiler_fingerprint="host-cc",
            cross_compiler_fingerprint="cross-cc",
            mode="release",
            stmm_fingerprint="bbb",
        )

        self.assertEqual(plan_a.cert_create_key, plan_b.cert_create_key)
        self.assertEqual(plan_a.bl31_key, plan_b.bl31_key)
        self.assertNotEqual(plan_a.tee_key, plan_b.tee_key)

    def test_cache_plan_changes_when_mode_changes(self) -> None:
        release = cix_release_cache.build_cache_plan(
            cert_create_tree_fingerprint="cert-tree",
            tfa_tree_fingerprint="tfa-tree",
            tee_tree_fingerprint="tee-tree",
            helper_fingerprint="helper",
            host_compiler_fingerprint="host-cc",
            cross_compiler_fingerprint="cross-cc",
            mode="release",
            stmm_fingerprint="stmm",
        )
        debug = cix_release_cache.build_cache_plan(
            cert_create_tree_fingerprint="cert-tree",
            tfa_tree_fingerprint="tfa-tree",
            tee_tree_fingerprint="tee-tree",
            helper_fingerprint="helper",
            host_compiler_fingerprint="host-cc",
            cross_compiler_fingerprint="cross-cc",
            mode="debug",
            stmm_fingerprint="stmm",
        )

        self.assertEqual(release.cert_create_key, debug.cert_create_key)
        self.assertEqual(release.tee_key, debug.tee_key)
        self.assertNotEqual(release.bl31_key, debug.bl31_key)

    def test_shell_assignments_match_expected_layout(self) -> None:
        plan = cix_release_cache.CachePlan(
            cert_create_key="cert",
            bl31_key="bl31",
            tee_key="tee",
        )
        with tempfile.TemporaryDirectory() as tmpdir_text:
            cache_root = Path(tmpdir_text)
            rendered = cix_release_cache.emit_shell_assignments(cache_root, plan)
            self.assertIn(
                f"CERT_CREATE_CACHE_BIN={shlex.quote(str(cache_root / 'cert_create' / 'cert' / 'cert_create'))}",
                rendered,
            )
            self.assertIn(
                f"BL31_CACHE_BIN={shlex.quote(str(cache_root / 'bl31' / 'bl31' / 'bl31.bin'))}",
                rendered,
            )
            self.assertIn(
                f"TEE_CACHE_BIN={shlex.quote(str(cache_root / 'tee' / 'tee' / 'tee-raw.bin'))}",
                rendered,
            )


if __name__ == "__main__":
    unittest.main()
