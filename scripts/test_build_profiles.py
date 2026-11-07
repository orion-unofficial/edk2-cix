#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path
import unittest

from build_profiles import resolve_profile
from reconstruction_common import ReconstructionError


REPO_ROOT = Path(__file__).resolve().parents[1]


class BuildProfileTests(unittest.TestCase):
    def test_default_profile_is_exact_current_upstream_replay(self) -> None:
        profile = resolve_profile(REPO_ROOT)

        self.assertEqual(profile["profile"], "upstream")
        self.assertEqual(profile["build_kind"], "deterministic-replay")
        self.assertEqual(profile["release"], "edk2-202208/radxa-1.3.2")
        self.assertEqual(profile["replay_version"], "1.3.2")
        self.assertEqual(profile["artefact_mode"], "upstream")
        self.assertEqual(profile["enable_firmware_fixes"], "false")
        self.assertEqual(profile["cix_early_boot_release"], "")

    def test_latest_profile_uses_maintained_source_without_fixes(self) -> None:
        profile = resolve_profile(REPO_ROOT, requested_profile="latest")

        self.assertEqual(profile["build_kind"], "source-build")
        self.assertEqual(profile["release"], "edk2-202608/radxa-1.3.2/unofficial-1.3.2")
        self.assertEqual(profile["artefact_mode"], "custom")
        self.assertEqual(profile["enable_firmware_fixes"], "false")
        self.assertEqual(profile["cix_early_boot_release"], "")

    def test_help_and_replay_defaults_follow_policy_without_source_refs(self) -> None:
        with tempfile.TemporaryDirectory(prefix="policy-help-") as directory:
            repo = Path(directory)
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            shutil.copy2(REPO_ROOT / "Makefile", repo / "Makefile")
            shutil.copytree(REPO_ROOT / "scripts", repo / "scripts",
                            ignore=shutil.ignore_patterns("__pycache__"))
            (repo / "config").mkdir()
            policy = json.loads((REPO_ROOT / "config/policies.json").read_text())
            policy["firmware_profile_policy"]["profiles"]["upstream"]["replay_version"] = "1.3.7"
            source = policy["unofficial_source_policy"]
            source["lines"][source["default_line"]].update(
                current_edk2_release="202702", current_radxa_release="1.3.6")
            (repo / "config/policies.json").write_text(json.dumps(policy))
            wrapper = repo / "python-probe.py"
            wrapper.write_text(
                "import os,sys\n"
                "from pathlib import Path\n"
                "with Path('calls').open('a') as f: f.write(sys.argv[1]+'\\n')\n"
                "os.execv(sys.executable,[sys.executable,*sys.argv[1:]])\n")
            with (repo / "Makefile").open("a") as stream:
                stream.write("\npolicy-probe:\n\t@echo $(REPLAY_VERSION) $(REPLAY_SOURCE_TARGET)\n")

            def make(*args):
                return subprocess.check_output(
                    ["make", "--no-print-directory", f"PYTHON={sys.executable} {wrapper}", *args],
                    cwd=repo, text=True)
            text = " ".join(make("help", "help-vars").split())
            self.assertIn("Radxa 1.3.7", text)
            self.assertIn("EDK2 202702/Radxa 1.3.6", text)
            self.assertIn("Policy default: 1.3.7.", text)
            self.assertEqual((repo / "calls").read_text().splitlines(),
                             ["scripts/build_profiles.py"])
            (repo / "calls").unlink()
            make("help-dev")
            self.assertFalse((repo / "calls").exists())
            upstream = subprocess.check_output(
                [sys.executable, "scripts/build_profiles.py", "--profile", "upstream", "--field", "release"],
                cwd=repo, text=True)
            self.assertEqual(upstream.strip(), "edk2-202208/radxa-1.3.7")
            workflow = (REPO_ROOT / ".github/workflows/deterministic-replay.yaml").read_text()
            start = workflow.index('          replay_version="${REPLAY_VERSION}"')
            end = workflow.index('          release_json=', start)
            resolver = textwrap.dedent(workflow[start:end]) + '\necho "$replay_version"\n'
            for version, target, expected_version, expected_target in (
                ("", "", "1.3.7", "edk2-202208/radxa-1.3.7"),
                ("1.3.1", "", "1.3.1", "edk2-202208/radxa-1.3.1"),
                ("1.3.1", "historical-target", "1.3.1", "historical-target"),
            ):
                output = repo / "workflow-output"
                output.unlink(missing_ok=True)
                result = subprocess.check_output(
                    ["bash", "-ec", resolver], cwd=repo, text=True,
                    env={**os.environ, "REPLAY_VERSION": version,
                         "REPLAY_SOURCE_TARGET": target, "GITHUB_OUTPUT": str(output)})
                self.assertEqual(result.strip(), expected_version)
                self.assertEqual(output.read_text().strip(), f"source_target={expected_target}")
            self.assertEqual(make("policy-probe").strip(), "1.3.7 edk2-202208/radxa-1.3.7")
            self.assertEqual(make("policy-probe", "REPLAY_VERSION=1.3.1").strip(),
                             "1.3.1 edk2-202208/radxa-1.3.1")

    def test_latest_profile_allows_explicit_fixes(self) -> None:
        profile = resolve_profile(
            REPO_ROOT,
            requested_profile="latest",
            firmware_fixes_override="true",
        )

        self.assertEqual(profile["enable_firmware_fixes"], "true")

    def test_upstream_profile_accepts_explicit_false_but_rejects_active_custom_options(self) -> None:
        profile = resolve_profile(
            REPO_ROOT,
            firmware_fixes_override="false",
            custom_options=["DEBUG_ON_UART3=false"],
        )
        self.assertEqual(profile["enable_firmware_fixes"], "false")

        with self.assertRaisesRegex(ReconstructionError, "ENABLE_FIRMWARE_FIXES=true"):
            resolve_profile(REPO_ROOT, firmware_fixes_override="true")
        with self.assertRaisesRegex(ReconstructionError, "DEBUG_ON_UART3"):
            resolve_profile(REPO_ROOT, custom_options=["DEBUG_ON_UART3=true"])

    def test_profile_rejects_semantically_conflicting_low_level_overrides(self) -> None:
        with self.assertRaisesRegex(ReconstructionError, "requires ARTEFACT_MODE=upstream"):
            resolve_profile(REPO_ROOT, artefact_mode_override="custom")
        with self.assertRaisesRegex(ReconstructionError, "requires FIRMWARE_TARGET=RELEASE"):
            resolve_profile(REPO_ROOT, firmware_target="DEBUG")
        with self.assertRaisesRegex(ReconstructionError, "fixes RELEASE"):
            resolve_profile(REPO_ROOT, release_override="some/other/target")


if __name__ == "__main__":
    unittest.main()
