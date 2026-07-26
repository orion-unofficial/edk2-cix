#!/usr/bin/env python3

from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile
import unittest

from source_normalisation import (
    attribute_inconsistent_paths,
    modified_tracked_paths,
    normalise_worktree,
)


class SourceNormalisationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.repo = pathlib.Path(tempfile.mkdtemp(prefix="edk2-cix-source-normalisation-test."))
        subprocess.run(["git", "-C", str(self.repo), "init", "-b", "main"], check=True, capture_output=True)
        subprocess.run(
            ["git", "-C", str(self.repo), "config", "user.name", "Source Normalisation Test"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(self.repo), "config", "user.email", "source-normalisation-test"],
            check=True,
        )

    def tearDown(self) -> None:
        shutil.rmtree(self.repo)

    def add(self, path: str, content: bytes, mode: int = 0o644) -> None:
        target = self.repo / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        target.chmod(mode)
        subprocess.run(["git", "-C", str(self.repo), "add", "--", path], check=True)

    def index_mode(self, path: str) -> str:
        output = subprocess.run(
            ["git", "-C", str(self.repo), "ls-files", "-s", "--", path],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        return output.split(" ", 1)[0]

    def test_normalises_text_without_touching_binary_or_scripts(self) -> None:
        self.add("source.c", b"one \r\ntwo\t\r\n")
        self.add("data.txt", b"not a script\r\n", 0o755)
        self.add("tool.sh", b"#!/usr/bin/env bash\r\nprintf ok\r\n", 0o755)
        self.add("firmware.bin", b"\x7fELF\r\n\0payload", 0o755)

        result = normalise_worktree(self.repo)

        self.assertEqual(result.line_endings, 3)
        self.assertEqual(result.trailing_whitespace, 1)
        self.assertEqual(result.file_modes, 1)
        self.assertEqual((self.repo / "source.c").read_bytes(), b"one\ntwo\n")
        self.assertEqual((self.repo / "data.txt").read_bytes(), b"not a script\n")
        self.assertEqual(self.index_mode("data.txt"), "100644")
        self.assertEqual(self.index_mode("tool.sh"), "100755")
        self.assertEqual(self.index_mode("firmware.bin"), "100755")
        self.assertEqual((self.repo / "firmware.bin").read_bytes(), b"\x7fELF\r\n\0payload")

    def test_is_idempotent_and_can_be_limited_to_changed_paths(self) -> None:
        self.add("selected.txt", b"selected\r\n")
        self.add("untouched.txt", b"untouched\r\n")

        first = normalise_worktree(self.repo, paths=("selected.txt",))
        second = normalise_worktree(self.repo, paths=("selected.txt",))

        self.assertEqual(first.line_endings, 1)
        self.assertEqual(first.trailing_whitespace, 0)
        self.assertEqual(second.changed, 0)
        self.assertEqual((self.repo / "selected.txt").read_bytes(), b"selected\n")
        self.assertEqual((self.repo / "untouched.txt").read_bytes(), b"untouched\r\n")

    def test_selected_paths_are_processed_in_bounded_batches(self) -> None:
        selected = [f"nested/path-{index:04d}.txt" for index in range(256)]
        for path in selected:
            self.add(path, b"selected\r\n")

        result = normalise_worktree(self.repo, paths=selected)

        self.assertEqual(result.line_endings, len(selected))
        self.assertTrue(
            all((self.repo / path).read_bytes() == b"selected\n" for path in selected)
        )

    def test_tracked_ignored_paths_can_be_normalised(self) -> None:
        self.add(".gitignore", b"ignored/\n")
        target = self.repo / "ignored/tracked.txt"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"tracked\r\n")
        subprocess.run(
            ["git", "-C", str(self.repo), "add", "-f", "--", "ignored/tracked.txt"],
            check=True,
        )

        result = normalise_worktree(self.repo, paths=("ignored/tracked.txt",))

        self.assertEqual(result.line_endings, 1)
        self.assertEqual(target.read_bytes(), b"tracked\n")

    def test_modified_tracked_paths_excludes_untracked_files(self) -> None:
        self.add("tracked.txt", b"before\n")
        subprocess.run(
            ["git", "-C", str(self.repo), "commit", "-m", "tracked file"],
            check=True,
            capture_output=True,
        )
        (self.repo / "tracked.txt").write_bytes(b"after\n")
        (self.repo / "untracked.txt").write_bytes(b"untracked\n")

        self.assertEqual(modified_tracked_paths(self.repo), ["tracked.txt"])

    def test_attribute_inconsistent_paths_finds_crlf_index_with_lf_policy(self) -> None:
        self.add(".gitattributes", b"*.txt text eol=lf\n")
        self.add("bad.txt", b"placeholder\n")
        subprocess.run(
            ["git", "-C", str(self.repo), "commit", "-m", "line-ending policy"],
            check=True,
            capture_output=True,
        )
        blob = subprocess.run(
            ["git", "-C", str(self.repo), "hash-object", "-w", "--stdin"],
            input=b"bad\r\n",
            check=True,
            capture_output=True,
            text=False,
        ).stdout.decode("ascii").strip()
        subprocess.run(
            [
                "git",
                "-C",
                str(self.repo),
                "update-index",
                "--cacheinfo",
                "100644",
                blob,
                "bad.txt",
            ],
            check=True,
        )
        (self.repo / "bad.txt").write_bytes(b"bad\r\n")

        self.assertEqual(attribute_inconsistent_paths(self.repo), ["bad.txt"])


if __name__ == "__main__":
    unittest.main()
