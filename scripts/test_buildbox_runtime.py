#!/usr/bin/env python3
"""Exercise source-carried buildbox runtime selection without a container daemon."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from reconstruction_common import show_file

ROOT = Path(__file__).resolve().parents[1]


def configured_targets():
    """Cover checkpoints, compatibility branches and both active lines."""
    manifest = json.loads((ROOT / "config/refs-unofficial.json").read_text())
    return tuple(record["ref"] for record in manifest["refs"])


class BuildboxRuntimeTests(unittest.TestCase):
    def test_runtime_selection(self):
        # SOURCE_TEST_REF also permits exercising an exact candidate/rendered tree.
        refs = (os.environ["SOURCE_TEST_REF"],) if os.environ.get("SOURCE_TEST_REF") else configured_targets()
        cases = (
            ("docker works", 0, 0, "", "docker", "", "Darwin"),
            ("docker info fails", 1, 0, "", "podman", "falling back to podman", "Darwin"),
            ("docker absent", None, 0, "", "podman", "", "Darwin"),
            ("explicit docker", 1, 0, "docker", "docker", "", "Darwin"),
            ("explicit podman", 0, 0, "podman", "podman", "", "Darwin"),
            ("explicit skips probing", 0, 1, "podman", "podman", "", "Darwin"),
            ("both info fail", 1, 1, "", None, "None of the installed runtimes", "Darwin"),
            ("both absent", None, None, "", None, "Neither podman nor docker", "Darwin"),
            ("linux real docker", 0, 0, "", "docker", "", "Linux"),
            ("linux podman only", None, 0, "", "podman", "", "Linux"),
            ("linux docker info fails", 1, 0, "", "podman", "falling back to podman", "Linux"),
        )
        implementations = {}
        for ref in refs:
            runner = show_file(ROOT, ref, "scripts/run_in_buildbox.sh")
            helper = show_file(ROOT, ref, "scripts/default_container_runtimes.sh")
            # Read every source; exercise each distinct complete helper pair.
            digest = hashlib.sha256(runner + b"\0" + helper).hexdigest()
            implementations.setdefault(digest, (ref, runner.decode(), helper))
        for digest, (ref, runner, helper_bytes) in implementations.items():
            functions = []
            for name in ("status", "resolve_container_runtime", "runtime"):
                match = re.search(r"^" + name + r"\(\) \{\n.*?^\}", runner, re.M | re.S)
                self.assertIsNotNone(match, name)
                functions.append(match.group())
            for label, docker, podman, explicit, expected, diagnostic, host_os in cases:
                with self.subTest(ref=ref, implementation=digest, case=label), tempfile.TemporaryDirectory(prefix="buildbox-runtime-") as directory:
                    work = Path(directory)
                    # Restrict PATH so a host Docker/Podman cannot affect the test.
                    for tool in ("bash", "cat", "grep"):
                        (work / tool).symlink_to(shutil.which(tool))
                    for name, info_status in (("docker", docker), ("podman", podman)):
                        if info_status is None:
                            continue
                        mock = work / name
                        mock.write_text(
                            "#!/bin/bash\n"
                            'case "$1" in\n'
                            f'  info) echo \'{{"Server":"{name} Engine"}}\'; exit {info_status} ;;\n'
                            '  version) echo \'{"Version":"test"}\' ;;\n'
                            f'  probe) printf "invoked:{name}:%s\\n" "$1" ;;\n'
                            '  *) exit 9 ;;\n'
                            'esac\n'
                        )
                        mock.chmod(0o755)
                    helper = work / "default_container_runtimes.sh"
                    helper.write_bytes(helper_bytes)
                    env = dict(os.environ, PATH=str(work), EDK2_CIX_HOST_OS=host_os,
                               EDK2_CIX_CONTAINER_RUNTIME=explicit, HELPER=str(helper))
                    script = (
                        'set -euo pipefail\n'
                        'container_runtime="${EDK2_CIX_CONTAINER_RUNTIME:-}"\n'
                        'default_runtime_helper="$HELPER"\n'
                        + "\n".join(functions)
                        + '\ncontainer_runtime="$(resolve_container_runtime)"\n'
                        + 'printf "%s\\n" "$container_runtime"\nruntime probe\n'
                    )
                    result = subprocess.run([shutil.which("bash"), "-c", script], env=env,
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0 if expected else 1, result.stderr)
                    if expected:
                        # Exact equality rejects logs, embedded newlines and whitespace.
                        self.assertEqual(result.stdout, f"{expected}\ninvoked:{expected}:probe\n")
                    else:
                        self.assertEqual(result.stdout, "")
                    if diagnostic:
                        self.assertIn(diagnostic, result.stderr)
                        self.assertTrue(result.stderr.startswith("[buildbox] "))
                    else:
                        self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
