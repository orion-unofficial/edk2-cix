#!/usr/bin/env python3
"""Execute retained Make recipes and check their AArch64 LTO search paths.

SOURCE_LTO_CANDIDATES may name a JSON list of source commit IDs; SOURCE_TEST_REPO
selects the repository containing those commits. Otherwise check current lines.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SourceLtoFlagsTests(unittest.TestCase):
    def test_effective_linker_paths_for_both_targets_and_fix_states(self):
        repo = Path(os.environ.get("SOURCE_TEST_REPO", ROOT))
        candidates = os.environ.get("SOURCE_LTO_CANDIDATES")
        refs = (json.loads(Path(candidates).read_text()) if candidates else
                ["source/unofficial/1.2/current", "source/unofficial/1.3/current"])
        with tempfile.TemporaryDirectory(prefix="source-lto-") as temporary:
            root = Path(temporary)
            validator = root / "layout.py"
            validator.write_text("print(1)\n")
            for ref in refs:
                def git(*args):
                    return subprocess.check_output(["git", "-C", str(repo), *args])
                source = git("show", ref + ":src/Makefile").decode()
                paths = set(git("ls-tree", "-r", "--name-only", ref).decode().splitlines())
                start = source.index('\tif [[ "$(ARTEFACT_MODE)" == "custom" ]]; then \\\n\t\tlto_flag=')
                recipe = source[start:source.index('\tif [[ "$${#tool_def_overrides[@]}"', start)]
                tag = re.search(r"build\s+-a\s+AARCH64\s+-t\s+(\w+)", source).group(1)
                for mode in ("custom", "upstream"):
                    for fixes in ("TRUE", "FALSE"):
                        with self.subTest(ref=ref, mode=mode, fixes=fixes):
                            values = dict(ARTEFACT_MODE=mode, ENABLE_FIRMWARE_FIXES_NORMALIZED=fixes,
                                          ENABLE_CORE_ORDER_NORMALIZED="cix", V="0",
                                          CUSTOM_OVERLAY_ROOT=str(root / "custom/overlay"),
                                          FIRMWARE_CHAIN_VALIDATOR=str(validator), REPO_ROOT=str(root),
                                          WORKSPACE="/workspaces/edk2-cix/src/edk2", DEBUG_ALLOW_LARGE_IMAGE="0")
                            probe = root / "recipe.mk"
                            probe.write_text("SHELL := bash\n.ONESHELL:\n.SHELLFLAGS := -eu -c\n"
                                             + "".join(f"{k} := {v}\n" for k, v in values.items())
                                             + "check:\n\t@tool_def_overrides=(); \\\n" + recipe
                                             + '\tprintf "%s\\n" "$${tool_def_overrides[@]}"\n')
                            result = subprocess.run(["make", "--no-print-directory", "-f", str(probe)],
                                                    text=True, capture_output=True)
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                            flags = dict(line.strip().split(" = ", 1)
                                         for line in result.stdout.splitlines() if line.strip())
                            flags = {key.strip(): value for key, value in flags.items()}
                            if mode == "upstream":
                                self.assertEqual(flags, {})
                                continue
                            for target in ("DEBUG", "RELEASE"):
                                linker = shlex.split(flags[f"{target}_{tag}_AARCH64_DLINK_FLAGS"])
                                libraries = [p.removeprefix("-L$(WORKSPACE)/") for p in linker
                                             if p.startswith("-L$(WORKSPACE)/")]
                                self.assertEqual(len(libraries), 1)
                                archive = "src/edk2/" + libraries[0] + "/liblto-aarch64.a"
                                self.assertTrue(archive in paths, f"{ref}: missing {archive}")
                                self.assertIn("-llto-aarch64", linker)
                                self.assertIn("-Wl,-plugin-opt=-pass-through=-llto-aarch64", linker)


if __name__ == "__main__":
    unittest.main()
