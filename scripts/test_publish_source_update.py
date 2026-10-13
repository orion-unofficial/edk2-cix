#!/usr/bin/env python3
"""Regression tests for guarded build/source publication."""

from __future__ import annotations

from contextlib import redirect_stdout
from io import StringIO
import json
import tempfile
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

from publish_source_update import (
    ORION_PUSH_URLS,
    check_publication_identity,
    compatibility_tag,
    infer_pending_refs,
    local_commit,
    main,
    metadata_ref,
    mutable_ref,
    orion_email,
    verify_metadata,
)
from reconstruction_common import ReconstructionError, git as publication_git
from test_support import commit_all, git, run, write_file


class PublishSourceUpdateTests(unittest.TestCase):
    def test_obsolete_remote_correction_blocks_publication_without_deleting_it(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, remote = root / "repo", root / "remote.git"
            git(root, "init", "-q", "--bare", str(remote))
            git(root, "init", "-q", "-b", "build", str(repo))
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            write_file(repo, "config/refs-unofficial.json", json.dumps({"refs": []}))
            tip = commit_all(repo, "build")
            obsolete = "refs/heads/source/unofficial/corrections/1.3.1/edk2-stable202608/fix"
            git(repo, "remote", "add", "origin", str(remote))
            git(repo, "push", "-q", "origin", "build", f"{tip}:{obsolete}")
            before = git(repo, "ls-remote", "origin").stdout
            with patch("publish_source_update.repo_root", return_value=repo), patch(
                "sys.argv", ["publish_source_update.py", "--write", "1"]
            ), self.assertRaisesRegex(ReconstructionError, "obsolete remote correction"):
                main()
            self.assertEqual(git(repo, "ls-remote", "origin").stdout, before)

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

    def test_dry_run_uses_one_atomic_push_for_build_and_source(self) -> None:
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
                    '}]}'
                    "\n"
                ),
            )
            old_build = commit_all(repo, "old metadata")
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
                    '}]}'
                    "\n"
                ),
            )
            new_build = commit_all(repo, "new metadata")

            output = StringIO()
            with patch("publish_source_update.repo_root", return_value=repo), patch(
                "sys.argv",
                ["publish_source_update.py", "--remote", "origin", "--write", "0"],
            ), redirect_stdout(output):
                main()

            publication = output.getvalue()
            self.assertIn("publication command: git push --atomic", publication)
            self.assertIn(f"{new_build}:refs/heads/build", publication)
            self.assertIn(
                f"{new_source}:refs/heads/{branch}",
                publication,
            )
            self.assertEqual(
                git(repo, "ls-remote", "origin", "refs/heads/build").stdout.split()[0],
                old_build,
            )
            self.assertEqual(
                git(repo, "ls-remote", "origin", f"refs/heads/{branch}").stdout.split()[0],
                old_source,
            )

    def test_existing_immutable_source_ref_cannot_be_replaced(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "repo"
            remote = root / "remote.git"
            git(root, "init", "-q", "--bare", str(remote))
            git(root, "init", "-q", "-b", "build", str(repo))
            git(repo, "config", "user.name", "Publication Test")
            git(repo, "config", "user.email", "publication-test")
            git(repo, "remote", "add", "origin", str(remote))

            branch = "source/base/edk2/edk2-stable202608"
            write_file(repo, "firmware.txt", "published\n")
            published_source = commit_all(repo, "published source")
            git(repo, "branch", branch, published_source)
            published_tree = git(repo, "rev-parse", f"{branch}^{{tree}}").stdout.strip()
            write_file(
                repo,
                "config/refs-edk2.json",
                (
                    '{"refs":[{"immutable":true,"object_id":"'
                    f'{published_source}","ref":"{branch}",'
                    f'"tree_id":"{published_tree}"}}]'
                    "}\n"
                ),
            )
            commit_all(repo, "published metadata")
            git(repo, "push", "-q", "origin", "build", branch)

            git(repo, "switch", "-q", branch)
            write_file(repo, "firmware.txt", "replacement\n")
            replacement = commit_all(repo, "replacement source")
            replacement_tree = git(repo, "rev-parse", "HEAD^{tree}").stdout.strip()
            git(repo, "switch", "-q", "build")
            write_file(
                repo,
                "config/refs-edk2.json",
                (
                    '{"refs":[{"immutable":true,"object_id":"'
                    f'{replacement}","ref":"{branch}",'
                    f'"tree_id":"{replacement_tree}"}}]'
                    "}\n"
                ),
            )
            commit_all(repo, "replacement metadata")

            with self.assertRaisesRegex(
                ReconstructionError,
                "refusing to replace immutable source ref",
            ), patch("publish_source_update.repo_root", return_value=repo), patch(
                "sys.argv",
                ["publish_source_update.py", "--remote", "origin", "--write", "0", "--force", "1"],
            ):
                main()


class PublicationIdentityTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory(prefix="publication-identity-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.repo = self.root / "repo"
        self.remote = self.root / "remote.git"
        git(self.root, "init", "-q", "--bare", str(self.remote))
        git(self.root, "init", "-q", "-b", "build", str(self.repo))
        git(self.repo, "config", "user.name", "Orion Unofficial")
        git(self.repo, "config", "user.email", "orion-unofficial@protonmail.com")
        git(self.repo, "remote", "add", "origin", str(self.remote))
        self.protected = patch("publish_source_update.ORION_PUSH_URLS", {str(self.remote)})
        self.protected.start()
        self.addCleanup(self.protected.stop)
        # Existing upstream identities must not prevent later publication.
        self.old = self.commit("existing upstream", "upstream@example.invalid", "upstream@example.invalid")
        git(self.repo, "push", "-q", "origin", "build")

    def commit(self, message: str, author: str = "orion-unofficial@protonmail.com",
               committer: str = "orion-unofficial@protonmail.com") -> str:
        run(["git", "commit", "--allow-empty", "-m", message], self.repo, env={
            "GIT_AUTHOR_NAME": "Orion Unofficial", "GIT_AUTHOR_EMAIL": author,
            "GIT_COMMITTER_NAME": "Orion Unofficial", "GIT_COMMITTER_EMAIL": committer,
        })
        return git(self.repo, "rev-parse", "HEAD").stdout.strip()

    def check(self, *tips: str, force: str = "0") -> None:
        specs = [f"{tip}:refs/heads/source/unofficial/test-{i}" for i, tip in enumerate(tips)]
        check_publication_identity(self.repo, "origin", specs, force)

    def test_known_emails_and_exact_destination_urls(self) -> None:
        self.assertEqual(ORION_PUSH_URLS, {
            "https://orion-unofficial@github.com/orion-unofficial/edk2-cix.git",
            "https://github.com/orion-unofficial/edk2-cix.git",
        })
        for email in ("orion-unofficial@protonmail.com", "orion-unofficial@users.noreply.github.com",
                      "275841278+orion-unofficial@users.noreply.github.com"):
            self.assertTrue(orion_email(email), email)
        for email in ("other@example.invalid", "orion-unofficial@example.invalid",
                      "other+" + "orion-unofficial@users.noreply.github.com",
                      "275841278+orion-unofficial@users.noreply.github.com".replace("275841278", "12345"),
                      "275841278+orion-unofficial@users.noreply.github.com".replace("orion-unofficial", "other")):
            self.assertFalse(orion_email(email), email)

    def test_existing_upstream_history_and_duplicate_tips_are_excluded(self) -> None:
        new = self.commit("new Orion commit")
        git(self.repo, "tag", "-a", "annotated", "-m", "tag", new)
        output = StringIO()
        with redirect_stdout(output):
            self.check(new, "refs/tags/annotated", new)
        self.assertIn("passed for 1 new commit(s)", output.getvalue())
        # A new ref pointing to history already on another remote ref is safe.
        with redirect_stdout(output):
            self.check(self.old)
        self.assertIn("passed for 0 new commit(s)", output.getvalue())

    def test_account_noreply_emails_are_accepted_in_commit_metadata(self) -> None:
        new = self.commit("noreply identity", author="orion-unofficial@users.noreply.github.com",
                          committer="275841278+orion-unofficial@users.noreply.github.com")
        self.check(new)

    def test_both_requested_https_urls_enforce_identity(self) -> None:
        bad = self.commit("wrong identity", author="other@example.invalid")
        for url in sorted(ORION_PUSH_URLS):
            def destination_git(repo, *args, **kwargs):
                if args == ("remote", "get-url", "--push", "--all", "origin"):
                    return subprocess.CompletedProcess(args, 0, url + "\n", "")
                return publication_git(repo, *args, **kwargs)

            with self.subTest(url=url), patch("publish_source_update.ORION_PUSH_URLS", ORION_PUSH_URLS), patch(
                "publish_source_update.git", side_effect=destination_git
            ), patch("publish_source_update.remote_objects", return_value={"refs/heads/build": self.old}) as query, self.assertRaisesRegex(
                ReconstructionError, "identity/user error"
            ):
                self.check(bad)
            query.assert_called_once_with(self.repo, url, [])

    def test_bad_author_or_committer_and_hidden_ancestors_are_reported(self) -> None:
        author = self.commit("wrong author", author="other@example.invalid")
        committer = self.commit("wrong committer", committer="other@example.invalid")
        tip = self.commit("good tip")
        for force in ("0", "true", "yes"):
            with self.subTest(force=force), self.assertRaises(ReconstructionError) as caught:
                self.check(tip, force=force)
            message = str(caught.exception)
            self.assertIn("identity/user error", message)
            self.assertIn(f"{author}: author Orion Unofficial <other@example.invalid>", message)
            self.assertIn(f"{committer}: committer Orion Unofficial <other@example.invalid>", message)
            self.assertIn("FORCE=1", message)

    def test_mailmap_and_current_user_settings_do_not_hide_bad_metadata(self) -> None:
        bad = self.commit("wrong identity", author="other@example.invalid", committer="other@example.invalid")
        write_file(self.repo, ".mailmap", "Orion Unofficial <orion-unofficial@protonmail.com> <other@example.invalid>\n")
        with self.assertRaisesRegex(ReconstructionError, bad):
            self.check(bad)

    def test_replace_refs_cannot_hide_bad_metadata(self) -> None:
        bad = self.commit("wrong identity", author="other@example.invalid")
        replacement = self.commit("replacement identity")
        git(self.repo, "replace", bad, replacement)
        with self.assertRaisesRegex(ReconstructionError, bad):
            self.check(bad)

    def test_all_merge_parents_are_checked(self) -> None:
        git(self.repo, "switch", "-q", "-c", "topic")
        bad = self.commit("topic identity", committer="other@example.invalid")
        git(self.repo, "switch", "-q", "build")
        self.commit("good mainline")
        git(self.repo, "merge", "--no-ff", "topic", "-m", "merge")
        with self.assertRaisesRegex(ReconstructionError, bad):
            self.check("HEAD")

    def test_only_force_one_bypasses_the_check(self) -> None:
        bad = self.commit("wrong identity", author="other@example.invalid")
        with patch("publish_source_update.remote_objects") as query:
            self.check(bad, force="1")
        query.assert_not_called()

    def test_other_destinations_and_deletions_are_not_checked(self) -> None:
        bad = self.commit("wrong identity", author="other@example.invalid")
        with patch("publish_source_update.ORION_PUSH_URLS", set()), patch(
            "publish_source_update.remote_objects"
        ) as query:
            self.check(bad)
        query.assert_not_called()
        with patch("publish_source_update.remote_objects") as query:
            check_publication_identity(self.repo, "origin", [":refs/heads/obsolete"], "0")
        query.assert_not_called()

    def test_effective_push_url_is_used_including_multiple_push_urls(self) -> None:
        new = self.commit("new Orion commit")
        git(self.repo, "remote", "set-url", "origin", "https://example.org/fetch.git")
        git(self.repo, "config", "--add", "remote.origin.pushurl", "https://example.org/push.git")
        git(self.repo, "config", "--add", "remote.origin.pushurl", str(self.remote))
        self.check(new)

    def test_push_url_rewrites_are_resolved(self) -> None:
        new = self.commit("new Orion commit")
        git(self.repo, "remote", "set-url", "origin", "publication:test")
        git(self.repo, "config", f"url.{self.remote}.pushInsteadOf", "publication:test")
        self.check(new)

    def test_missing_remote_history_is_fetched_without_creating_refs(self) -> None:
        git(self.root, "init", "-q", "-b", "build", str(self.root / "fresh"))
        self.repo = self.root / "fresh"
        git(self.repo, "remote", "add", "origin", str(self.remote))
        new = self.commit("unrelated Orion root")
        before = git(self.repo, "show-ref").stdout
        self.check(new)
        self.assertEqual(git(self.repo, "show-ref").stdout, before)
        self.assertEqual(git(self.repo, "cat-file", "-t", self.old).stdout.strip(), "commit")
        self.assertFalse((self.repo / ".git/FETCH_HEAD").exists())

    def test_shallow_history_fails_closed(self) -> None:
        git(self.root, "clone", "-q", "--depth=1", "--branch=build",
            self.remote.as_uri(), str(self.root / "shallow"))
        self.repo = self.root / "shallow"
        git(self.repo, "remote", "set-url", "origin", str(self.remote))
        new = self.commit("new Orion commit")
        with self.assertRaisesRegex(ReconstructionError, "shallow repository"):
            self.check(new)

    def test_source_branch_and_annotated_tag_are_checked_during_resume(self) -> None:
        branch = "source/unofficial/edk2-stable202608"
        git(self.repo, "switch", "-q", "-c", branch)
        bad = self.commit("bad source identity", committer="other@example.invalid")
        git(self.repo, "tag", "-a", "source/unofficial/edk2/stable-202608", "-m", "source tag")
        tree = git(self.repo, "rev-parse", "HEAD^{tree}").stdout.strip()
        git(self.repo, "switch", "-q", "build")
        write_file(self.repo, "config/refs-unofficial.json", json.dumps({"refs": [{
            "ref": branch, "object_id": bad, "tree_id": tree, "immutable": False,
        }]}) + "\n")
        commit_all(self.repo, "good build metadata")
        for resume in (False, True):
            if resume:
                git(self.repo, "push", "-q", "origin", "build")
            with self.subTest(resume=resume), patch("publish_source_update.repo_root", return_value=self.repo), patch(
                "sys.argv", ["publisher", "--write", "1", "--force", "0"]
            ), self.assertRaisesRegex(ReconstructionError, bad):
                main()
            self.assertEqual(git(self.repo, "ls-remote", "origin", "refs/heads/source/*", "refs/tags/source/*").stdout, "")

    def test_main_aborts_before_push_in_dry_run_and_write_modes(self) -> None:
        self.commit("wrong identity", author="other@example.invalid")
        # Empty metadata gives a focused build-only publication transaction.
        for write in ("0", "1"):
            with self.subTest(write=write), patch("publish_source_update.repo_root", return_value=self.repo), patch(
                "sys.argv", ["publisher", "--write", write, "--force", "0"]
            ), self.assertRaisesRegex(ReconstructionError, "identity/user error"):
                main()
            self.assertEqual(git(self.repo, "ls-remote", "origin", "refs/heads/build").stdout.split()[0], self.old)
        with patch("publish_source_update.repo_root", return_value=self.repo), patch(
            "sys.argv", ["publisher", "--write", "1", "--force", "1"]
        ):
            main()
        self.assertEqual(git(self.repo, "ls-remote", "origin", "refs/heads/build").stdout.split()[0],
                         git(self.repo, "rev-parse", "HEAD").stdout.strip())

    def test_make_passes_force_to_the_publisher(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        result = subprocess.run(["make", "-n", "publish-source-update", "WRITE=1", "FORCE=1"],
                                cwd=repo, text=True, capture_output=True, check=True)
        recipe = next(line for line in result.stdout.splitlines() if "scripts/publish_source_update.py" in line)
        self.assertIn('FORCE="1"', recipe)


if __name__ == "__main__":
    unittest.main()
