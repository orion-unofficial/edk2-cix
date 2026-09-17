#!/usr/bin/env python3
"""Regression coverage for sibling modules hidden by partial overlays."""

import os
from pathlib import Path
import tempfile
import unittest

from check_source_build_inputs import BUILD_FIXES, missing_build_fixes, missing_module_infs, missing_package_declarations, missing_platform_inputs, missing_toolchain, source_input_problems
from test_support import commit_all, git, write_file


class SourceBuildInputsTests(unittest.TestCase):
    def test_inf_package_dependencies_follow_upstream_package_removal(self) -> None:
        infs = {"Driver.inf": "[Packages]\nSignedCapsulePkg/SignedCapsulePkg.dec\n[Sources]\nignored.dec\n"}
        self.assertEqual(missing_package_declarations({"src/edk2/SignedCapsulePkg/SignedCapsulePkg.dec"}, infs), [])
        self.assertEqual(len(missing_package_declarations(set(), infs)), 1)
        infs["Driver.inf"] = "[Packages]\nMdeModulePkg/MdeModulePkg.dec"
        self.assertEqual(missing_package_declarations({"src/edk2/MdeModulePkg/MdeModulePkg.dec"}, infs), [])

    def test_removed_toolchain_is_rejected_but_historical_toolchains_are_valid(self) -> None:
        old_make = b"build -a AARCH64 -t GCC5 -p board.dsc"
        old_tools = b"DEFINE GCC5_AARCH64_CC_FLAGS = flags\r\n"
        new_tools = b"DEFINE GCC_AARCH64_CC_FLAGS = flags\r\n"
        self.assertEqual(missing_toolchain(old_make, old_tools), [])
        self.assertEqual(len(missing_toolchain(old_make, new_tools)), 1)
        self.assertEqual(missing_toolchain(old_make.replace(b"GCC5", b"GCC"), new_tools), [])

    def test_platform_include_chain_rejects_removed_library(self) -> None:
        descriptors = {
            f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc": "!include Platform/CIX/Common.dsc.inc"
            for board in ("O6", "O6N")
        }
        descriptors["src/edk2-platforms/Platform/CIX/Common.dsc.inc"] = (
            "[LibraryClasses]\nCpuExceptionHandlerLib|ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf\n"
            "!if $(DISABLED_FEATURE) == TRUE\nOtherLib|Absent/Other.inf\n!endif\n"
        )
        paths = set(descriptors)
        problems = missing_platform_inputs(paths, descriptors, ())
        self.assertEqual(len(problems), 1)
        self.assertIn("ArmExceptionLib.inf", problems[0])
        paths.add("src/edk2/ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf")
        self.assertEqual(missing_platform_inputs(paths, descriptors, ()), [])

    def test_platform_inputs_honor_overlay_precedence(self) -> None:
        base = {f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc": "!include Platform/Common.dsc.inc" for board in ("O6", "O6N")}
        base["src/edk2-platforms/Platform/Common.dsc.inc"] = "Lib|Pkg/Removed.inf"
        overlay = "custom/overlay/edk2-platforms/Platform/Common.dsc.inc"
        base[overlay] = "Lib|Pkg/Present.inf"
        paths = set(base) | {"src/edk2/Pkg/Present.inf"}
        self.assertEqual(missing_platform_inputs(paths, base, ("custom/overlay",)), [])
        base[overlay] = "!include Platform/Missing.dsc.inc"
        self.assertEqual(missing_platform_inputs(paths, base, ("custom/overlay",)), ["missing platform input: Platform/Missing.dsc.inc"])

    def test_each_required_build_fix_is_checked_independently(self) -> None:
        contents = {}
        for _, path, marker, _ in BUILD_FIXES:
            contents[path] = contents.get(path, b"") + marker + b"\n"
        self.assertEqual(missing_build_fixes(contents), [])
        for commit, path, marker, _ in BUILD_FIXES:
            broken = dict(contents)
            broken[path] = broken[path].replace(marker, b"missing")
            self.assertTrue(any(commit in problem for problem in missing_build_fixes(broken)))
        contents.pop("scripts/ensure_iasl.sh")
        self.assertEqual(missing_build_fixes(contents), [])

    def test_one_inf_does_not_cover_another_module_in_the_same_directory(self) -> None:
        directory = "edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/"
        paths = {
            "src/" + directory + "FwVersionDxe.inf",
            "src/" + directory + "FwVersionProtocolTest.inf",
            "custom/overlay/" + directory + "FwVersionDxe.inf",
        }
        missing = "custom/overlay/" + directory + "FwVersionProtocolTest.inf"
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), [missing])
        paths.add(missing)
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), [])

    def test_unshadowed_modules_and_unrelated_directories_remain_valid(self) -> None:
        paths = {"src/edk2/Pkg/Driver/A.inf", "custom/overlay/edk2/Pkg/Other/file.c"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), [])

    def test_library_sibling_does_not_generate_ffs_inf_prerequisite(self) -> None:
        library = "src/edk2/Pkg/Library/Runtime.inf"
        paths = {library, "custom/overlay/edk2/Pkg/Library/Boot.inf"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay", {library}), [])

    def test_experimental_overlay_is_checked_independently(self) -> None:
        paths = {"src/edk2/Pkg/Driver/A.inf", "custom/overlay-experimental-uefi-settings/edk2/Pkg/Driver/file.c"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay-experimental-uefi-settings"), [
            "custom/overlay-experimental-uefi-settings/edk2/Pkg/Driver/A.inf",
        ])

    def test_nested_data_directory_also_shadows_module_root(self) -> None:
        paths = {"src/edk2/Pkg/Driver/A.inf", "custom/overlay/edk2/Pkg/Driver/Data/settings.bin"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), ["custom/overlay/edk2/Pkg/Driver/A.inf"])

    def test_git_tree_audit_resolves_both_overlay_layers_and_rejects_broken_links(self) -> None:
        with tempfile.TemporaryDirectory(prefix="source-input-links.") as directory:
            repo = Path(directory)
            git(repo, "init", "-b", "build")
            git(repo, "config", "user.name", "Test")
            git(repo, "config", "user.email", "test")
            relative = "edk2/Pkg/Driver/A.inf"
            write_file(repo, "src/" + relative, "[Defines]\n  MODULE_TYPE = DXE_DRIVER\n")
            normal = repo / "custom/overlay" / relative
            experimental = repo / "custom/overlay-experimental-uefi-settings" / relative
            for path, target in ((normal, repo / "src" / relative), (experimental, normal)):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.symlink_to(os.path.relpath(target, path.parent))
            commit_all(repo, "valid chained overlays")
            self.assertFalse(any("symlink" in problem for problem in source_input_problems(repo, "HEAD")))
            experimental.unlink()
            experimental.symlink_to("missing.inf")
            commit_all(repo, "broken overlay")
            self.assertTrue(any("unresolvable overlay symlink" in problem for problem in source_input_problems(repo, "HEAD")))


if __name__ == "__main__":
    unittest.main()
