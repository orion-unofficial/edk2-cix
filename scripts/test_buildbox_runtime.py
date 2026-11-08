#!/usr/bin/env python3
"""Exercise source-carried buildbox runtime selection without a container daemon."""

from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
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
        central_runner = (ROOT / "scripts/run_in_buildbox.sh").read_bytes()
        central_helper = (ROOT / "scripts/default_container_runtimes.sh").read_bytes()
        central_digest = hashlib.sha256(central_runner + b"\0" + central_helper).hexdigest()
        implementations = {central_digest: ('build', central_runner.decode(), central_helper)}
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


class BuildboxLockTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="buildbox-locks-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.workspace = self.root / "firmware"
        self.workspace.mkdir()
        self.workspace_lock = self.workspace / ".buildbox/locks"
        self.cache_lock = self.root / "cache/locks"
        self.engine_marker = self.root / "engine-called"
        engine = self.root / "engine"
        engine.write_text('#!/bin/bash\nprintf called >> "$ENGINE_MARKER"\nexit 1\n')
        engine.chmod(0o755)
        self.env = {key: value for key, value in os.environ.items()
                    if not key.startswith("EDK2_CIX_")}
        self.env.update(EDK2_CIX_FIRMWARE_ROOT=str(self.workspace),
                        EDK2_CIX_BUILD_LOCK_ROOT=str(self.cache_lock),
                        EDK2_CIX_CONTAINER_RUNTIME=str(engine), ENGINE_MARKER=str(self.engine_marker))

    @contextmanager
    def hold(self, lock_root):
        ready = self.root / "ready"
        ready.unlink(missing_ok=True)
        process = subprocess.Popen([
            str(ROOT / 'scripts/with_repo_lock.sh'), 'bash', '-c',
            'printf ready > "$1"; read -r line', 'hold', str(ready)],
            env={**self.env, 'EDK2_CIX_BUILD_LOCK_ROOT': str(lock_root)},
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 3
            while not ready.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(ready.exists(), 'lock holder did not become ready')
            yield
        finally:
            process.communicate('\n' if process.poll() is None else None, timeout=3)
            self.assertEqual(process.returncode, 0)

    def run_runner(self, **values):
        self.engine_marker.unlink(missing_ok=True)
        return subprocess.run([str(ROOT / 'scripts/run_in_buildbox.sh'), 'true'],
                              env={**self.env, **values}, capture_output=True, text=True, timeout=5)

    def test_build_caller_honours_an_existing_direct_worktree_lock(self):
        with self.hold(self.workspace_lock):
            result = self.run_runner()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Another build-related command', result.stderr)
        self.assertFalse(self.engine_marker.exists())

    def test_different_workspaces_still_honour_the_shared_cache_lock(self):
        with self.hold(self.cache_lock):
            result = self.run_runner()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Another build-related command', result.stderr)
        self.assertFalse(self.engine_marker.exists())
        self.assertFalse((self.workspace_lock / 'repo-build.lock.d').exists())

    def test_a_foreign_repository_lock_does_not_bypass_the_workspace_lock(self):
        foreign = self.root / 'other-checkout/locks'
        foreign.mkdir(parents=True)
        with self.hold(self.workspace_lock):
            result = self.run_runner(EDK2_CIX_REPO_LOCK_HELD='1', EDK2_CIX_REPO_LOCK_ROOT=str(foreign))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Another build-related command', result.stderr)
        self.assertFalse(self.engine_marker.exists())

    def test_direct_worktree_caller_does_not_acquire_its_held_lock_twice(self):
        with self.hold(self.workspace_lock):
            result = self.run_runner(EDK2_CIX_REPO_LOCK_HELD='1',
                                     EDK2_CIX_BUILD_LOCK_ROOT=str(self.workspace_lock) + '/.')
        # The deliberately failing engine proves that both lock gates were passed.
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.engine_marker.exists(), result.stderr)
        self.assertNotIn('Another build-related command', result.stderr)

    def test_legacy_custom_cache_lock_cannot_wait_in_reverse_order(self):
        with self.hold(self.workspace_lock):
            # A second holder needs a distinct readiness marker.
            other = self.root / 'cache-ready'
            process = subprocess.Popen([
                str(ROOT / 'scripts/with_repo_lock.sh'), 'bash', '-c',
                'printf ready > "$1"; read -r line', 'hold', str(other)], env=self.env,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 3
                while not other.exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(other.exists())
                result = self.run_runner(EDK2_CIX_REPO_LOCK_HELD='1', EDK2_CIX_BUILD_LOCK_WAIT='1')
            finally:
                process.communicate('\n' if process.poll() is None else None, timeout=3)
                self.assertEqual(process.returncode, 0)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Another build-related command', result.stderr)
        self.assertFalse(self.engine_marker.exists())


if __name__ == "__main__":
    unittest.main()
