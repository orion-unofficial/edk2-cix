#!/usr/bin/env python3

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "src" / "scripts" / "resolve_build_metadata.sh"


def run(
    argv: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    merged_env = os.environ.copy()
    if env is not None:
        merged_env.update(env)

    return subprocess.run(
        argv,
        cwd=cwd,
        env=merged_env,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def git(repo: Path, *args: str) -> str:
    return run(["git", *args], cwd=repo).stdout.strip()


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


class BuildMetadataResolutionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.repo = Path(tempfile.mkdtemp(prefix="edk2-cix-build-metadata-test."))
        git(self.repo, "init", "-b", "main")
        git(self.repo, "config", "user.name", "Build Metadata Test")
        git(self.repo, "config", "user.email", "build-metadata-test@example.invalid")

        script_copy = self.repo / "src" / "scripts" / "resolve_build_metadata.sh"
        script_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(SCRIPT, script_copy)

    def tearDown(self) -> None:
        shutil.rmtree(self.repo)

    def commit_all(self, *message_args: str) -> str:
        git(self.repo, "add", ".")
        git(self.repo, "commit", *message_args)
        return git(self.repo, "rev-parse", "HEAD")

    def run_metadata(
        self,
        *args: str,
        env: dict[str, str] | None = None,
    ) -> str:
        result = run(
            ["bash", "src/scripts/resolve_build_metadata.sh", *args],
            cwd=self.repo,
            env=env,
        )
        return result.stdout.strip()

    def test_component_commit_prefers_explicit_replay_environment(self) -> None:
        write(self.repo / "src" / "edk2" / "file.txt", "edk2\n")
        self.commit_all("-m", "initial")

        explicit = "0123456789abcdef0123456789abcdef01234567"

        self.assertEqual(
            self.run_metadata(
                "component-commit",
                "edk2",
                env={"EDK2_SOURCE_COMMIT": explicit},
            ),
            explicit,
        )

    def test_component_commit_prefers_origin_trailer_when_present(self) -> None:
        write(self.repo / "src" / "edk2" / "file.txt", "edk2\n")
        self.commit_all("-m", "initial")

        origin = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        write(self.repo / "README.md", "origin\n")
        self.commit_all(
            "-m",
            "curated import",
            "-m",
            f"Origin-Repo: radxa/edk2\nOrigin-Commit: {origin}",
        )

        self.assertEqual(
            self.run_metadata("component-commit", "edk2"),
            origin,
        )

    def test_component_commit_falls_back_to_component_path_history(self) -> None:
        write(self.repo / "src" / "edk2" / "file.txt", "edk2\n")
        expected = self.commit_all("-m", "component update")

        write(self.repo / "README.md", "docs\n")
        self.commit_all("-m", "unrelated update")

        self.assertEqual(
            self.run_metadata("component-commit", "edk2"),
            expected,
        )

    def test_upstream_build_metadata_resolves_without_origin_trailers(self) -> None:
        result = subprocess.run(
            [
                "make",
                "--no-print-directory",
                "-C",
                "src",
                "print-build-metadata",
                "ARTEFACT_MODE=upstream",
                "FIRMWARE_BOARD=O6",
                "FIRMWARE_TARGET=RELEASE",
            ],
            cwd=REPO_ROOT,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        values: dict[str, str] = {}
        for line in result.stdout.splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                values[key] = value

        for key in (
            "EDK2_COMMIT_HASH",
            "EDK2_NON_OSI_COMMIT_HASH",
            "EDK2_PLATFORMS_COMMIT_HASH",
        ):
            self.assertTrue(values.get(key), f"expected {key} to resolve")


if __name__ == "__main__":
    unittest.main()
