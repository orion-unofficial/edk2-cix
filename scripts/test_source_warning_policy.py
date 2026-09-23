#!/usr/bin/env python3
"""Exercise the warning policy from the retained firmware source, including Bison."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from reconstruction_common import for_each_ref, show_file

ROOT = Path(__file__).resolve().parents[1]
FILES = (
    "scripts/ensure_iasl.sh",
    "scripts/test_ensure_iasl.py",
    "scripts/test_filter_edk2_build_output.py",
    "src/scripts/filter_edk2_build_output.py",
    "containers/replay-bookworm/packages.bookworm-amd64.txt",
    "containers/replay-bookworm/packages.bookworm-arm64.txt",
)


class SourceWarningPolicyTests(unittest.TestCase):
    def test_effective_autogen_warning_arguments_survive_release_setup(self):
        selected = os.environ.get("SOURCE_TEST_REF")
        local = os.environ.get("SOURCE_TEST_ROOT")
        refs = [selected] if selected else for_each_ref(ROOT, "source/unofficial/")
        if local:
            refs = ["local"]
        with tempfile.TemporaryDirectory(prefix="autogen-arguments-") as tmp:
            (Path(tmp) / "tools_def.txt").write_text("# test configuration\n")
            for ref in refs:
                source = (Path(local) / "src/Makefile").read_text() if local else show_file(ROOT, ref, "src/Makefile").decode()
                # EDK2 retains this AArch64 exception for mixed BASE/XIP and
                # DXE alignment contexts, which can warn for identical types.
                self.assertEqual(source.count("-Wno-lto-type-mismatch"), 2, ref)
                tools = (Path(local) / "src/edk2/BaseTools/Conf/tools_def.template").read_text() if local else show_file(ROOT, ref, "src/edk2/BaseTools/Conf/tools_def.template").decode()
                self.assertIn("-Wno-lto-type-mismatch", tools, ref)
                self.assertIn("-Werror", tools, ref)
                start = source.index("\tbuild_extra_defines=();")
                recipe = source[start:source.index("\tbuild_version_defines=();", start)]
                recipe = recipe.replace("\\\n", "\n").replace("$$", "$")
                for mode in ("custom", "upstream"):
                    for target in ("RELEASE", "DEBUG"):
                        with self.subTest(ref=ref, mode=mode, target=target):
                            script = recipe.replace("$(ARTEFACT_MODE)", mode).replace("$(UEFI_TARGET)", target)
                            self.assertNotIn("$(", script)
                            script += '\nprintf "%s\\n" "${build_extra_defines[@]}"\n'
                            result = subprocess.run(["bash", "-eu", "-c", script],
                                                    env={**os.environ, "CONF_PATH": tmp}, capture_output=True, text=True)
                            self.assertEqual(result.returncode, 0, result.stderr)
                            arguments = result.stdout.splitlines()
                            self.assertEqual("-w" in arguments, mode == "custom")
                            self.assertEqual("REPRODUCIBLE_BUILD_METADATA=TRUE" in arguments,
                                             mode == "custom" and target == "RELEASE")

    def test_real_source_warning_regressions(self):
        with tempfile.TemporaryDirectory(prefix="source-warning-policy-") as tmp:
            root = Path(tmp)
            for relative in FILES:
                source_root = os.environ.get("SOURCE_TEST_ROOT")
                if source_root:
                    data = (Path(source_root) / relative).read_bytes()
                else:
                    ref = os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current")
                    data = show_file(ROOT, ref, relative)
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                path.chmod(0o755)
            regressions = {
                "test_ensure_iasl.py": "test_bison_extension_is_allowed_but_other_warnings_fail",
                "test_filter_edk2_build_output.py": "test_drops_debuglink_noop_output",
            }
            for name, method in regressions.items():
                self.assertIn("def " + method + "(", (root / "scripts" / name).read_text())
                result = subprocess.run(
                    [sys.executable, str(root / "scripts" / name)],
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            # Unknown diagnostics must stay visible in both modes.
            warning = "module.c:10: warning: an unexpected compiler warning\n"
            for mode in ("custom", "upstream"):
                result = subprocess.run(
                    [sys.executable, str(root / "src/scripts/filter_edk2_build_output.py")],
                    input=warning, capture_output=True, text=True,
                    env={**os.environ, "ARTEFACT_MODE": mode, "V": "0"},
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, warning)

            known = "/work/src/edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc(199): warning: /work/src/edk2-non-osi/Silicon/CIX/Sky1/Drivers/DpuDxe/DpuDxe.inf does not support LIBRARY_CLASS DpuDxe\n"
            for mode, verbose, debug in (("upstream", "0", "0"), ("upstream", "1", "0"),
                                         ("upstream", "0", "1"), ("custom", "0", "0")):
                text = known + known.replace("LIBRARY_CLASS DpuDxe", "LIBRARY_CLASS NewClass") + warning
                result = subprocess.run(
                    [sys.executable, str(root / "src/scripts/filter_edk2_build_output.py")],
                    input=text, capture_output=True, text=True,
                    env={**os.environ, "ARTEFACT_MODE": mode, "V": verbose, "DEBUG": debug},
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, text[len(known):] if (mode, verbose, debug) == ("upstream", "0", "0") else text)

            failure = "ld: error: cannot fix LOAD segment with RWX permissions\n"
            result = subprocess.run(
                [sys.executable, str(root / "src/scripts/filter_edk2_build_output.py")],
                input=failure, capture_output=True, text=True,
                env={**os.environ, "ARTEFACT_MODE": "upstream", "V": "0", "DEBUG": "0"},
            )
            self.assertEqual(result.stdout, failure)


if __name__ == "__main__":
    unittest.main()
