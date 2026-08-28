#!/usr/bin/env python3
"""Regression checks for atomic source-ref publication."""

import argparse
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import subprocess
import unittest
from unittest.mock import Mock, patch

import publish_source_update
from reconstruction_common import ReconstructionError


def completed(
    args: tuple[str, ...] | list[str],
    output: str = "",
    returncode: int = 0,
):
    return subprocess.CompletedProcess(args, returncode, output, "")


class PublishSourceUpdateTests(unittest.TestCase):
    source_ref = "refs/heads/source/base/edk2/edk2-stable202608"

    def arguments(self, write: str = "0") -> argparse.Namespace:
        return argparse.Namespace(
            source_refs=self.source_ref,
            remote="origin",
            write=write,
            v="0",
        )

    def git_result(self, _repo: Path, *args: str, **_kwargs):
        if args[:4] == ("symbolic-ref", "--quiet", "--short", "HEAD"):
            return completed(args, "build\n")
        if args[:2] == ("status", "--porcelain"):
            return completed(args)
        if args[:2] == ("rev-parse", "refs/heads/build"):
            return completed(args, "local-build\n")
        if args[:2] == ("rev-parse", f"{self.source_ref}^{{commit}}"):
            return completed(args, "source-object\n")
        if args[:2] == ("rev-parse", f"{self.source_ref}^{{tree}}"):
            return completed(args, "source-tree\n")
        if args[:3] == ("rev-parse", "--verify", self.source_ref):
            return completed(args, "source-object\n")
        if args[:2] == ("rev-parse", self.source_ref):
            return completed(args, "source-object\n")
        if args[0] == "fetch":
            return completed(args)
        raise AssertionError(f"unexpected git call: {args}")

    def run_main(self, *, write: str = "0", remote_source: str | None = None):
        pushed: list[list[str]] = []

        def subprocess_run(command, **_kwargs):
            if command[:3] == ["git", "-C", "/repository"]:
                return completed(
                    command,
                    returncode=1 if "diff" in command else 0,
                )
            if command[:3] == ["git", "push", "--atomic"]:
                pushed.append(command)
                return completed(command)
            raise AssertionError(f"unexpected subprocess call: {command}")

        remote_refs = {"refs/heads/build": "remote-build"}
        if remote_source is not None:
            remote_refs[self.source_ref] = remote_source
        record = {
            "manifest": "config/refs-edk2.json",
            "object_id": "source-object",
            "ref": self.source_ref.removeprefix("refs/heads/"),
            "tree_id": "source-tree",
        }
        parser = Mock()
        parser.parse_args.return_value = self.arguments(write)
        with (
            patch.object(publish_source_update, "parser", return_value=parser),
            patch.object(
                publish_source_update,
                "repo_root",
                return_value=Path("/repository"),
            ),
            patch.object(
                publish_source_update,
                "remote_objects",
                return_value=remote_refs,
            ),
            patch.object(
                publish_source_update,
                "load_ref_records",
                return_value=[record],
            ),
            patch.object(
                publish_source_update,
                "git",
                side_effect=self.git_result,
            ),
            patch.object(
                publish_source_update.subprocess,
                "run",
                side_effect=subprocess_run,
            ),
            redirect_stdout(StringIO()),
        ):
            publish_source_update.main()
        return pushed

    def test_dry_run_uses_one_atomic_push_for_build_and_source(self) -> None:
        pushes = self.run_main()

        self.assertEqual(len(pushes), 1)
        command = pushes[0]
        self.assertIn("--atomic", command)
        self.assertIn("--dry-run", command)
        self.assertIn(
            "--force-with-lease=refs/heads/build:remote-build",
            command,
        )
        self.assertIn("refs/heads/build:refs/heads/build", command)
        self.assertIn(f"{self.source_ref}:{self.source_ref}", command)

    def test_existing_immutable_source_ref_cannot_be_replaced(self) -> None:
        with self.assertRaisesRegex(
            ReconstructionError,
            "refusing to replace immutable",
        ):
            self.run_main(remote_source="old-source-object")


if __name__ == "__main__":
    unittest.main()
