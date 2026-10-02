#!/usr/bin/env python3
"""Test bounded O6 normalization against retained source-era FDFs."""

from pathlib import Path
import unittest
from unittest.mock import patch

from radxa13_fdf import (
    EXPERIMENTAL_FDF, MIRROR, ORDINARY_FDF, SOURCE_FDF, canonical_fdf,
    expected_fdf, fdf_updates, normalize_source_fdf,
)
from reconstruction_common import ReconstructionError, git, resolve_ref, temp_dir
from validate_radxa13_source import Entry, GitTree, validate


ROOT = Path(__file__).resolve().parents[1]
RETAINED = (("202208", "1.3.1"), ("202605", "1.3.0"),
            ("202605", "1.3.1"), ("202608", "1.3.1"))


class BlobTree(GitTree):
    """Apply byte/mode mutations without writing objects or touching refs."""

    def __init__(self, source, changes=None, modes=None, removed=()):
        self.repo, self.revision = source.repo, source.revision
        self.entries = source.entries.copy()
        self.changes = changes or {}
        for path in removed:
            self.entries.pop(path)
        for path, mode in (modes or {}).items():
            self.entries[path] = Entry(mode, self.entries.get(path, Entry("", "")).oid)

    def blob(self, path):
        return self.changes[path] if path in self.changes else super().blob(path)


class Radxa13FdfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = {
            pair: GitTree(ROOT, resolve_ref(ROOT, f"source/unofficial/{pair[1]}/edk2-stable{pair[0]}"))
            for pair in RETAINED
        }

    def normalized(self, source):
        updates = fdf_updates(source)
        return BlobTree(source, {p: data for p, (_, data) in updates.items()},
                        {p: mode for p, (mode, _) in updates.items()})

    def test_all_retained_eras_normalize_and_pass_all_structural_checks(self):
        for pair, source in self.sources.items():
            with self.subTest(pair=pair):
                normalized = self.normalized(source)
                self.assertEqual({}, fdf_updates(normalized))
                self.assertEqual(expected_fdf(source), normalized.blob(ORDINARY_FDF))
                self.assertEqual(MIRROR, normalized.blob(EXPERIMENTAL_FDF))
                self.assertEqual(source.entries[SOURCE_FDF], normalized.entries[SOURCE_FDF])
                self.assertEqual(source.blob(SOURCE_FDF), normalized.blob(SOURCE_FDF))
                with patch("validate_radxa13_source.GitTree", return_value=normalized):
                    self.assertEqual([], validate(ROOT, "mutation", *pair))

    def test_real_normalization_is_child_commit_changes_only_two_overlay_paths(self):
        source = self.sources[("202605", "1.3.0")]
        before = git(ROOT, "rev-parse", source.revision).stdout.strip()
        # Read retained objects through an alternate, but write only into an
        # independent test object database. No shared repository object/ref
        # mutation is needed for this integration test.
        with temp_dir(ROOT, "radxa13-fdf-test-") as scratch:
            repo = Path(scratch)
            git(repo, "init", "-b", "test")
            git(repo, "config", "user.name", "FDF Test")
            git(repo, "config", "user.email", "fdf-test@example.invalid")
            objects = git(ROOT, "rev-parse", "--path-format=absolute", "--git-path", "objects").stdout.strip()
            (repo / ".git/objects/info/alternates").write_text(objects + "\n")
            (repo / "config").mkdir()
            (repo / "config/refs-edk2.json").write_bytes((ROOT / "config/refs-edk2.json").read_bytes())
            refs = git(repo, "for-each-ref", "--format=%(refname) %(objectname)").stdout
            after = normalize_source_fdf(repo, before, "202605", "1.3.0")
            self.assertEqual(before, git(repo, "rev-parse", after + "^").stdout.strip())
            self.assertEqual(sorted((ORDINARY_FDF, EXPERIMENTAL_FDF)),
                             git(repo, "diff", "--name-only", before, after).stdout.splitlines())
            self.assertEqual(source.entries[SOURCE_FDF], GitTree(repo, after).entries[SOURCE_FDF])
            self.assertEqual([], validate(repo, after, "202605", "1.3.0"))
            self.assertEqual(after, normalize_source_fdf(repo, after, "202605", "1.3.0"))
            self.assertEqual(refs, git(repo, "for-each-ref", "--format=%(refname) %(objectname)").stdout)
            message = git(repo, "show", "-s", "--format=%B", after).stdout
            self.assertIn("Source-Fdf-Input: " + before, message)
            self.assertIn("Source-Fdf-Source-Blob: " + source.entries[SOURCE_FDF].oid, message)

    def test_real_literal_vendor_and_port_layouts_produce_same_era_bodies(self):
        for edk2, ref in (
            ("202208", "source/vendor/radxa/1.3.1/edk2-stable202208"),
            ("202608", "source/port/radxa/1.3.1/edk2-stable202608"),
        ):
            with self.subTest(edk2=edk2):
                tree = GitTree(ROOT, resolve_ref(ROOT, ref))
                source = tree.blob(SOURCE_FDF)
                self.assertNotIn(b"[Defines]", source)
                self.assertEqual(expected_fdf(self.sources[(edk2, "1.3.1")]), canonical_fdf(source))

    def test_arm_gic_paths_are_preserved_across_source_eras(self):
        old = expected_fdf(self.sources[("202208", "1.3.1")])
        new = expected_fdf(self.sources[("202608", "1.3.1")])
        self.assertIn(b"ArmPkg/Drivers/ArmGic/ArmGicDxe.inf", old)
        self.assertIn(b"ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf", new)
        self.assertEqual(old.replace(b"ArmPkg/Drivers/ArmGic/", b"ArmPkg/Drivers/ArmGicDxe/"), new)

    def test_copying_modern_body_over_old_source_is_rejected(self):
        source = self.sources[("202208", "1.3.1")]
        modern = self.sources[("202608", "1.3.1")].blob(ORDINARY_FDF)
        with self.assertRaisesRegex(ReconstructionError, "unreviewed changes"):
            fdf_updates(BlobTree(source, {ORDINARY_FDF: modern}))

    def test_legitimate_custom_semantic_addition_requires_review(self):
        source = self.sources[("202605", "1.3.0")]
        data = source.blob(EXPERIMENTAL_FDF).replace(b"[FV.FvMain]", b"# retained custom setting\n[FV.FvMain]")
        with self.assertRaisesRegex(ReconstructionError, "unreviewed changes"):
            fdf_updates(BlobTree(source, {EXPERIMENTAL_FDF: data}))

    def test_unreviewed_layouts_and_capacities_fail_closed(self):
        source = self.sources[("202608", "1.3.1")].blob(SOURCE_FDF)
        for old, new in (
            (b"0x1f2", b"0x200"), (b"0x00400000", b"0x00800000"),
            (b"[FD.SKY1_BL33_UEFI]", b"[FD.OTHER]"),
            (b"0x84400000", b"0x84800000"), (b"0x00001000", b"0x00002000"),
            (b"!if $(TARGET) == DEBUG", b"!if $(TARGET) == RELEASE"),
            (b"FV = FVMAIN_COMPACT", b"FV = FVMAIN_COMPACT\n0x00200000|0x1000"),
            (b"ErasePolarity = 1", b"ErasePolarity = 1\nSize = 0x00200000"),
            (b"ErasePolarity = 1", b"ErasePolarity = 0"),
            (b"ErasePolarity = 1", b"[UNKNOWN]\nErasePolarity = 1"),
            (b"[FV.FvMain]", b"[FV.Other]"),
        ):
            with self.subTest(new=new), self.assertRaises(ReconstructionError):
                canonical_fdf(source.replace(old, new))

    def test_missing_or_macro_module_path_is_rejected(self):
        source = self.sources[("202208", "1.3.1")]
        for replacement in (b"ArmPkg/Drivers/Unknown/Unknown.inf", b"$(UNKNOWN_MODULE)"):
            data = source.blob(SOURCE_FDF).replace(b"ArmPkg/Drivers/ArmGic/ArmGicDxe.inf", replacement)
            with self.subTest(replacement=replacement), self.assertRaisesRegex(ReconstructionError, "module"):
                expected_fdf(BlobTree(source, {SOURCE_FDF: data}))

    def test_wrong_source_era_module_cannot_self_certify(self):
        source = self.sources[("202208", "1.3.1")]
        data = source.blob(SOURCE_FDF).replace(b"ArmPkg/Drivers/ArmGic/", b"ArmPkg/Drivers/ArmGicDxe/")
        with self.assertRaisesRegex(ReconstructionError, "missing O6 FDF source module"):
            expected_fdf(BlobTree(source, {SOURCE_FDF: data, ORDINARY_FDF: canonical_fdf(data)}))

    def test_ambiguous_and_escaping_mirrors_are_rejected(self):
        source = self.sources[("202208", "1.3.1")]
        with self.assertRaisesRegex(ReconstructionError, "ambiguous"):
            fdf_updates(BlobTree(source, modes={EXPERIMENTAL_FDF: "100644"}))
        with self.assertRaisesRegex(ValueError, "leaves source tree"):
            fdf_updates(BlobTree(source, {EXPERIMENTAL_FDF: b"../../../../../../../../../../escape"}))


if __name__ == "__main__":
    unittest.main()
