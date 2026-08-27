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
    def test_build_ci_fetches_complete_history_for_minimised_export(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "build-branch-ci.yaml").read_text(
            encoding="utf-8"
        )

        self.assertIn("fetch-depth: 0", text)

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

    def test_firmware_workflows_select_latest_source_explicitly(self) -> None:
        firmware = (REPO_ROOT / ".github" / "workflows" / "firmware-build.yaml").read_text(
            encoding="utf-8"
        )
        secure_boot = (REPO_ROOT / ".github" / "workflows" / "secure-boot-audit.yaml").read_text(
            encoding="utf-8"
        )

        self.assertIn("scripts/build_profiles.py --profile latest --field release", firmware)
        self.assertNotIn("artefact_mode:", firmware)
        self.assertIn('"ARTEFACT_MODE=custom"', firmware)
        self.assertIn('"CIX_RELEASE=v1.2"', firmware)
        self.assertIn('"ENABLE_FIRMWARE_FIXES=${ENABLE_FIRMWARE_FIXES_INPUT}"', firmware)
        self.assertIn("default: false\n        description: Enable the project's opinionated firmware fixes", firmware)
        self.assertEqual(
            secure_boot.count("scripts/build_profiles.py --profile latest --field release"),
            2,
        )
        self.assertEqual(secure_boot.count("ARTEFACT_MODE=custom"), 2)

    def test_no_firmware_workflow_depends_on_targetless_make(self) -> None:
        for name in ("deterministic-replay.yaml", "firmware-build.yaml", "secure-boot-audit.yaml"):
            with self.subTest(workflow=name):
                lines = (REPO_ROOT / ".github" / "workflows" / name).read_text(
                    encoding="utf-8"
                ).splitlines()
                self.assertFalse(any(line.strip() == "make" for line in lines))

    def test_deterministic_replay_defaults_to_exact_current_upstream_release(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "deterministic-replay.yaml").read_text(
            encoding="utf-8"
        )

        self.assertEqual(text.count("default: edk2-202208/radxa-1.3.1"), 1)
        self.assertEqual(text.count("default: 1.3.1"), 1)
        self.assertIn("inputs.replay_source_target || 'edk2-202208/radxa-1.3.1'", text)
        self.assertIn("inputs.replay_version || '1.3.1'", text)
        self.assertNotIn("unofficial-1.2.1", text)

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
        self.assertIn('$(cksum "$dockerfile"', runner)
        self.assertNotIn('printf \'%s\' "$repo_root" | cksum', runner)

    def test_local_act_removes_job_containers_and_container_owned_snapshots(self) -> None:
        runner = (REPO_ROOT / "scripts" / "run_github_actions_with_act.sh").read_text(
            encoding="utf-8"
        )

        self.assertIn("cleanup_act_workspace()", runner)
        self.assertIn('"${repo_root}/.cache/edk2-cix/act-workspaces/run."*', runner)
        self.assertIn(
            'if [[ "${1:-list}" == run || "$git_common_dir" != "$repo_root"/* ]]; then',
            runner,
        )
        self.assertIn('if [[ -n "$act_workspace" ]]; then', runner)
        self.assertIn('--env "EDK2_CIX_ACT_HOST_CACHE_ROOT=${act_host_cache_root}"', runner)
        self.assertIn('container_options="--volume=${git_common_dir}:${git_common_dir}:ro"', runner)
        self.assertIn("--entrypoint find", runner)
        self.assertIn("-mindepth 1 -depth -delete", runner)
        self.assertIn("args=(\n    --rm", runner)
        self.assertIn("--filter label=edk2-cix.buildbox.image", runner)
        self.assertIn('"${act_workspace}/"*', runner)
        self.assertIn('docker rm -f "$container_id"', runner)
        self.assertIn('status "Isolated CI snapshot: ${act_workspace}"', runner)
        self.assertIn('status "Persistent local cache: ${act_host_cache_root}"', runner)
        self.assertNotIn("Isolated linked-worktree snapshot", runner)
        self.assertNotIn("--volume=${repo_root}/.worktrees", runner)

        docs_runner = (REPO_ROOT / "docs" / "scripts" / "run_docs_workflow_local.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('host_cache_root="${EDK2_CIX_ACT_HOST_CACHE_ROOT:-}"', docs_runner)
        self.assertIn('-e EDK2_CIX_DOCS_CACHE_ROOT=/docs-cache', docs_runner)
        self.assertIn('-v "${host_cache_root}/docs:/docs-cache"', docs_runner)
        self.assertIn("cp -a /docs-cache/book/html", docs_runner)


if __name__ == "__main__":
    unittest.main()
