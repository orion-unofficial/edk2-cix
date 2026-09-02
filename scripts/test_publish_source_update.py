#!/usr/bin/env python3
"""Regression tests for guarded build/source publication."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from publish_source_update import (
    compatibility_tag,
    infer_pending_refs,
    local_commit,
    main,
    metadata_ref,
    mutable_ref,
    verify_metadata,
)
from reconstruction_common import ReconstructionError
from test_support import commit_all, git, write_file


class PublishSourceUpdateTests(unittest.TestCase):
    def test_mutable_unofficial_refs_have_direct_metadata_mappings(self) -> None:
        current = "refs/heads/source/unofficial/1.3/current"
        compatibility = "refs/heads/source/unofficial/edk2-stable202608"
        tag = "refs/tags/source/unofficial/edk2/stable-202608"

        self.assertEqual(metadata_ref(current), "source/unofficial/1.3/current")
        self.assertEqual(
            metadata_ref(compatibility),
            "source/unofficial/edk2-stable202608",
        )
        self.assertEqual(
            metadata_ref(tag),
            "source/unofficial/edk2-stable202608",
        )
        self.assertTrue(mutable_ref(current))
        self.assertTrue(mutable_ref(compatibility))
        self.assertTrue(mutable_ref(tag))
        self.assertEqual(
            compatibility_tag(compatibility),
            tag,
        )
        self.assertIsNone(compatibility_tag(current))

    def test_pending_refs_are_inferred_from_remote_manifest_differences(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            write_file(repo, "firmware.txt", "current\n")
            commit = commit_all(repo, "current")
            current = "source/unofficial/1.3/current"
            compatibility = "source/unofficial/edk2-stable202608"
            tag = "source/unofficial/edk2/stable-202608"
            git(repo, "branch", current, commit)
            git(repo, "branch", compatibility, commit)
            git(repo, "tag", tag, commit)
            tree = git(repo, "rev-parse", "HEAD^{tree}").stdout.strip()
            records = [
                {"ref": current, "object_id": commit, "tree_id": tree},
                {"ref": compatibility, "object_id": commit, "tree_id": tree},
            ]

            self.assertEqual(
                infer_pending_refs(repo, records, {}),
                [
                    f"refs/heads/{current}",
                    f"refs/heads/{compatibility}",
                    f"refs/tags/{tag}",
                ],
            )
            self.assertEqual(
                infer_pending_refs(
                    repo,
                    records,
                    {
                        f"refs/heads/{current}": commit,
                        f"refs/heads/{compatibility}": commit,
                        f"refs/tags/{tag}": commit,
                    },
                ),
                [],
            )

            git(repo, "switch", "-q", current)
            write_file(repo, "firmware.txt", "unrecorded\n")
            unrecorded = commit_all(repo, "unrecorded source move")
            self.assertEqual(local_commit(repo, f"refs/heads/{current}"), unrecorded)
            with self.assertRaisesRegex(ReconstructionError, "does not match build metadata"):
                infer_pending_refs(
                    repo,
                    records,
                    {
                        f"refs/heads/{current}": commit,
                        f"refs/heads/{compatibility}": commit,
                        f"refs/tags/{tag}": commit,
                    },
                )

    def test_mutable_ref_metadata_must_match_selected_object_and_tree(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            write_file(repo, "firmware.txt", "current\n")
            commit = commit_all(repo, "current")
            branch = "source/unofficial/1.3/current"
            git(repo, "branch", branch, commit)
            tree = git(repo, "rev-parse", f"{branch}^{{tree}}").stdout.strip()
            record = {
                "manifest": "config/refs-unofficial.json",
                "object_id": commit,
                "ref": branch,
                "tree_id": tree,
            }

            self.assertEqual(
                verify_metadata(repo, f"refs/heads/{branch}", [record]),
                "config/refs-unofficial.json",
            )

            write_file(repo, "firmware.txt", "moved\n")
            moved = commit_all(repo, "moved")
            git(repo, "branch", "-f", branch, moved)
            with self.assertRaisesRegex(ReconstructionError, "does not match"):
                verify_metadata(repo, f"refs/heads/{branch}", [record])

    def test_compatibility_tag_must_match_its_recorded_branch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q", "-b", "build")
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            write_file(repo, "firmware.txt", "tagged\n")
            tagged = commit_all(repo, "tagged")
            branch = "source/unofficial/edk2-stable202608"
            tag = "source/unofficial/edk2/stable-202608"
            git(repo, "branch", branch, tagged)
            git(repo, "tag", tag, tagged)
            write_file(repo, "firmware.txt", "branch moved\n")
            moved = commit_all(repo, "branch moved")
            git(repo, "branch", "-f", branch, moved)
            record = {
                "manifest": "config/refs-unofficial.json",
                "object_id": tagged,
                "ref": branch,
                "tree_id": git(
                    repo,
                    "rev-parse",
                    f"{tag}^{{tree}}",
                ).stdout.strip(),
            }

            with self.assertRaisesRegex(
                ReconstructionError,
                "tag does not identify its recorded compatibility branch",
            ):
                verify_metadata(repo, f"refs/tags/{tag}", [record])

    def test_publication_resumes_after_build_metadata_reaches_remote_first(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "repo"
            remote = root / "remote.git"
            git(root, "init", "-q", "--bare", str(remote))
            git(root, "init", "-q", "-b", "build", str(repo))
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            git(repo, "remote", "add", "origin", str(remote))

            branch = "source/unofficial/1.3/current"
            write_file(repo, "firmware.txt", "old\n")
            old_source = commit_all(repo, "old source")
            git(repo, "branch", branch, old_source)
            old_tree = git(repo, "rev-parse", f"{branch}^{{tree}}").stdout.strip()
            write_file(
                repo,
                "config/refs-unofficial.json",
                (
                    '{"refs":[{"immutable":false,"object_id":"'
                    f'{old_source}","ref":"{branch}","tree_id":"{old_tree}"'
                    '}]}\n'
                ),
            )
            commit_all(repo, "old metadata")
            git(repo, "push", "-q", "origin", "build", branch)

            git(repo, "switch", "-q", branch)
            write_file(repo, "firmware.txt", "new\n")
            new_source = commit_all(repo, "new source")
            new_tree = git(repo, "rev-parse", "HEAD^{tree}").stdout.strip()
            git(repo, "switch", "-q", "build")
            write_file(
                repo,
                "config/refs-unofficial.json",
                (
                    '{"refs":[{"immutable":false,"object_id":"'
                    f'{new_source}","ref":"{branch}","tree_id":"{new_tree}"'
                    '}]}\n'
                ),
            )
            commit_all(repo, "new metadata")
            git(repo, "push", "-q", "origin", "build")

            arguments = [
                "publish_source_update.py",
                "--remote",
                "origin",
                "--write",
                "0",
            ]
            with patch("publish_source_update.repo_root", return_value=repo), patch(
                "sys.argv", arguments
            ):
                main()

            self.assertEqual(
                git(repo, "ls-remote", "origin", f"refs/heads/{branch}").stdout.split()[0],
                old_source,
            )

            write_file(repo, "publisher.txt", "publisher fix\n")
            final_build = commit_all(repo, "publisher fix")
            arguments[-1] = "1"
            with patch("publish_source_update.repo_root", return_value=repo), patch(
                "sys.argv", arguments
            ):
                main()

            self.assertEqual(
                git(repo, "ls-remote", "origin", f"refs/heads/{branch}").stdout.split()[0],
                new_source,
            )
            self.assertEqual(
                git(repo, "ls-remote", "origin", "refs/heads/build").stdout.split()[0],
                final_build,
            )

            with patch("publish_source_update.repo_root", return_value=repo), patch(
                "sys.argv", arguments
            ):
                main()


if __name__ == "__main__":
    unittest.main()
