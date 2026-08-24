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

    def test_local_docs_use_the_reproducible_container_path(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "build-docs.yaml").read_text(encoding="utf-8")
        self.assertEqual(text.count("if: ${{ env.ACT != 'true' }}"), 5)
        self.assertIn("./docs/scripts/run_docs_workflow_local.sh", text)
        self.assertIn('${GITHUB_WORKSPACE}/.cache/edk2-cix/docs', text)
        for assignment in (
            'docs_repository_url="${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}"',
            'export MDBOOK_OUTPUT__HTML__EDIT_URL_TEMPLATE="${docs_repository_url}/edit/${docs_ref_name}/docs/{path}"',
            'export MDBOOK_OUTPUT__HTML__GIT_REPOSITORY_URL="${docs_repository_url}/tree/${docs_ref_name}"',
        ):
            self.assertIn(assignment, text)

        dockerfile = (REPO_ROOT / "docs" / "scripts" / "docs-workflow.Dockerfile").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "github:NixOS/nixpkgs/nixpkgs-unstable",
            dockerfile,
        )
        self.assertNotIn("channels.nixos.org", dockerfile)
        self.assertNotIn("profile install", dockerfile)

        runner = (REPO_ROOT / "docs" / "scripts" / "run_docs_workflow_local.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('docker build --progress plain "${build_args[@]}"', runner)


if __name__ == "__main__":
    unittest.main()
