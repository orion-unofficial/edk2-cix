#!/usr/bin/env python3
"""Regression checks for build-branch GitHub Actions portability."""

from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_WORKFLOWS = (
    "build-branch-ci.yaml",
    "deterministic-replay.yaml",
    "firmware-build.yaml",
    "secure-boot-audit.yaml",
    "upstream-versions.yaml",
)


class GitHubWorkflowTests(unittest.TestCase):
    def test_source_workflows_fetch_canonical_local_refs(self) -> None:
        for name in SOURCE_WORKFLOWS:
            with self.subTest(workflow=name):
                text = (REPO_ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
                self.assertIn("+refs/heads/source/*:refs/heads/source/*", text)
                self.assertNotIn("+refs/heads/source/*:refs/remotes/origin/source/*", text)

    def test_local_runs_skip_github_only_qemu_and_upload_actions(self) -> None:
        for name in ("deterministic-replay.yaml", "firmware-build.yaml", "secure-boot-audit.yaml"):
            with self.subTest(workflow=name):
                text = (REPO_ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
                self.assertIn("if: ${{ env.ACT != 'true' }}\n        uses: docker/setup-qemu-action@v4", text)
                self.assertIn("env.ACT != 'true'", text[text.index("uses: actions/upload-artifact@v7") - 120 :])


if __name__ == "__main__":
    unittest.main()
