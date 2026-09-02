#!/usr/bin/env python3
"""Regression checks for build-branch GitHub Actions portability."""

from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_WORKFLOWS = (
    "build-branch-ci.yaml",
    "deterministic-replay.yaml",
    "manual-firmware-build.yaml",
    "secure-boot-audit.yaml",
    "upstream-versions.yaml",
)


class GitHubWorkflowTests(unittest.TestCase):
    def test_build_ci_fetches_complete_history_for_minimised_export(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "build-branch-ci.yaml").read_text(
            encoding="utf-8"
        )

        self.assertIn("fetch-depth: 0", text)

    def test_build_ci_checks_remote_source_coherence_before_expensive_jobs(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "build-branch-ci.yaml").read_text(
            encoding="utf-8"
        )

        self.assertIn("source-coherence:", text)
        self.assertIn("make check-remote-source-coherence REMOTE=origin", text)
        self.assertEqual(text.count("      - source-coherence"), 4)
        self.assertEqual(
            text.count("needs:\n      - classify\n      - source-coherence"),
            4,
        )
        source_model = text[
            text.index("  source-model:") : text.index("\n  current-source:")
        ]
        self.assertIn(
            "if: ${{ always() && needs.classify.outputs.build_affecting == 'true' }}",
            source_model,
        )
        self.assertIn("make prepare-ci-source-refs WRITE=1", source_model)
        self.assertIn("continue-on-error: true", source_model)
        self.assertEqual(source_model.count("if: ${{ always() }}"), 4)
        self.assertIn('[[ "${PREPARE_OUTCOME}" == success ]]', source_model)

    def test_reusable_firmware_workflows_do_not_reuse_caller_concurrency(self) -> None:
        for name in ("deterministic-replay.yaml", "secure-boot-audit.yaml"):
            with self.subTest(workflow=name):
                text = (REPO_ROOT / ".github" / "workflows" / name).read_text(
                    encoding="utf-8"
                )
                self.assertIn("workflow_call:", text)
                self.assertNotIn(
                    "group: ${{ github.workflow }}-${{ github.run_id }}",
                    text,
                )

    def test_source_workflows_fetch_canonical_local_refs(self) -> None:
        for name in SOURCE_WORKFLOWS:
            with self.subTest(workflow=name):
                text = (REPO_ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
                self.assertIn("+refs/heads/source/*:refs/heads/source/*", text)
                self.assertNotIn("+refs/heads/source/*:refs/remotes/origin/source/*", text)

    def test_local_runs_skip_github_only_qemu_and_upload_actions(self) -> None:
        for name in ("deterministic-replay.yaml", "manual-firmware-build.yaml", "secure-boot-audit.yaml"):
            with self.subTest(workflow=name):
                text = (REPO_ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
                self.assertIn("if: ${{ env.ACT != 'true' }}\n        uses: docker/setup-qemu-action@v4", text)
                self.assertIn("env.ACT != 'true'", text[text.index("uses: actions/upload-artifact@v7") - 120 :])

    def test_parallel_firmware_jobs_isolate_worktrees_and_report_paths(self) -> None:
        replay = (REPO_ROOT / ".github" / "workflows" / "deterministic-replay.yaml").read_text(
            encoding="utf-8"
        )
        secure_boot = (REPO_ROOT / ".github" / "workflows" / "secure-boot-audit.yaml").read_text(
            encoding="utf-8"
        )

        self.assertIn("EDK2_CIX_WORKTREE_NAMESPACE: replay-${{ matrix.board }}", replay)
        self.assertIn(
            'artifact_root="${RUNNER_TEMP}/ci-artifacts/${{ matrix.board }}"',
            replay,
        )
        self.assertIn('path: ${{ runner.temp }}/ci-artifacts/**', replay)
        self.assertIn('[[ -z "${FIRMWARE_CACHE:-}" || -z "${FIRMWARE_WORKTREE:-}" ]]', replay)
        self.assertIn("EDK2_CIX_WORKTREE_NAMESPACE: metadata", secure_boot)
        self.assertIn(
            "EDK2_CIX_WORKTREE_NAMESPACE: secure-boot-${{ matrix.board }}-fixes-${{ matrix.firmware_fixes }}",
            secure_boot,
        )
        self.assertIn(
            'artifact_root="${RUNNER_TEMP}/ci-artifacts/${{ matrix.board }}-fixes-${{ matrix.firmware_fixes }}"',
            secure_boot,
        )
        self.assertIn('path: ${{ runner.temp }}/ci-artifacts/**', secure_boot)
        self.assertIn('[[ -z "${FIRMWARE_CACHE:-}" || -z "${FIRMWARE_WORKTREE:-}" ]]', secure_boot)
        self.assertIn("strategy:\n      fail-fast: false\n      max-parallel: 2", secure_boot)

    def test_firmware_workflows_select_latest_source_explicitly(self) -> None:
        firmware = (REPO_ROOT / ".github" / "workflows" / "manual-firmware-build.yaml").read_text(
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
        for name in ("deterministic-replay.yaml", "manual-firmware-build.yaml", "secure-boot-audit.yaml"):
            with self.subTest(workflow=name):
                lines = (REPO_ROOT / ".github" / "workflows" / name).read_text(
                    encoding="utf-8"
                ).splitlines()
                self.assertFalse(any(line.strip() == "make" for line in lines))

    def test_deterministic_replay_defaults_to_exact_current_upstream_release(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "deterministic-replay.yaml").read_text(
            encoding="utf-8"
        )

        self.assertEqual(text.count("default: edk2-202208/radxa-1.3.1"), 2)
        self.assertEqual(text.count("default: 1.3.1"), 2)
        self.assertIn("inputs.replay_source_target || 'edk2-202208/radxa-1.3.1'", text)
        self.assertIn("inputs.replay_version || '1.3.1'", text)
        self.assertNotIn("unofficial-1.2.1", text)

    def test_deterministic_replay_validates_once_and_collects_its_reports(self) -> None:
        text = (REPO_ROOT / ".github" / "workflows" / "deterministic-replay.yaml").read_text(
            encoding="utf-8"
        )

        self.assertEqual(text.count('make -C "${FIRMWARE_WORKTREE}" deterministic-replay'), 1)
        self.assertNotIn("buildbox-audit-final-image-manifest", text)
        self.assertNotIn("buildbox-audit-acpi-regression", text)
        self.assertNotIn("scripts/audit_final_image_manifest.py", text)
        self.assertIn('report_root="${FIRMWARE_WORKTREE}/build-validation"', text)
        self.assertIn('--report-root "${report_root}"', text)

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
        self.assertIn('ENV NIX_CONFIG="connect-timeout = 5"', dockerfile)
        self.assertIn('RES_OPTIONS="no-aaaa"', dockerfile)
        self.assertIn("nix --connect-timeout 60", dockerfile)

        runner = (REPO_ROOT / "docs" / "scripts" / "run_docs_workflow_local.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('docker build --progress plain "${build_args[@]}"', runner)
        self.assertIn('$(cksum "$dockerfile"', runner)
        self.assertNotIn('printf \'%s\' "$repo_root" | cksum', runner)

        build_runner = (REPO_ROOT / "docs" / "scripts" / "run_docs_build.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('if [[ "$in_container" == 1 ]]', build_runner)
        self.assertIn("--option cachix.enable:bool false", build_runner)
        self.assertIn("--option devenv.latestVersion:string 2.2.2", build_runner)
        self.assertIn('! "$binary" --version', build_runner)

        installer = (REPO_ROOT / "docs" / "scripts" / "install_mdbook_toc.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('&& "$binary" --version', installer)

    def test_local_act_removes_job_containers_and_container_owned_snapshots(self) -> None:
        runner = (REPO_ROOT / "scripts" / "run_github_actions_with_act.sh").read_text(
            encoding="utf-8"
        )

        self.assertIn("cleanup_act_workspace()", runner)
        self.assertIn('repository_root="$(dirname -- "$git_common_dir")"', runner)
        self.assertIn('${repository_root}/.cache/edk2-cix/act-cache', runner)
        self.assertIn('${repository_root}/.cache/edk2-cix}', runner)
        self.assertIn("prepare_action_cache()", runner)
        self.assertIn('python3 "$script_dir/check_remote_source_coherence.py" --remote origin', runner)
        self.assertIn("ACT_ALLOW_REMOTE_REF_DRIFT", runner)
        self.assertIn("this run is not proof of GitHub equivalence", runner)
        self.assertIn('git -C "$cache_dir" update-ref -d refs/heads/HEAD', runner)
        self.assertIn('git -C "$cache_dir" fetch --quiet --depth 1 origin "$action_ref"', runner)
        self.assertIn('git -c advice.detachedHead=false clone --quiet', runner)
        self.assertIn("args+=(--action-offline-mode)", runner)
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
        self.assertIn('--concurrent-jobs "$concurrent_jobs"', runner)
        self.assertIn('concurrent_jobs="${ACT_CONCURRENT_JOBS:-${EDK2_CIX_ACT_CONCURRENT_JOBS:-1}}"', runner)
        self.assertIn("done < <(docker ps -aq 2>/dev/null || true)", runner)
        self.assertNotIn("--filter label=edk2-cix.buildbox.image", runner)
        self.assertIn('"${act_workspace}/"*', runner)
        self.assertIn('docker rm -f "$container_id"', runner)
        self.assertIn('status "Isolated CI snapshot: ${act_workspace}"', runner)
        self.assertIn('status "Persistent local cache: ${act_host_cache_root}"', runner)
        self.assertIn('status "Concurrent jobs: ${concurrent_jobs}"', runner)
        self.assertNotIn("Isolated linked-worktree snapshot", runner)
        self.assertNotIn("--volume=${repo_root}/.worktrees", runner)

        docs_runner = (REPO_ROOT / "docs" / "scripts" / "run_docs_workflow_local.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn('host_cache_root="${EDK2_CIX_ACT_HOST_CACHE_ROOT:-}"', docs_runner)
        self.assertIn('-e EDK2_CIX_DOCS_CACHE_ROOT=/docs-cache', docs_runner)
        self.assertIn('-v "${host_cache_root}/docs:/docs-cache"', docs_runner)
        self.assertIn("cp -a /docs-cache/book/html", docs_runner)

        makefile = (REPO_ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("ACT_CONCURRENT_JOBS ?= 1", makefile)
        self.assertEqual(
            makefile.count('ACT_CONCURRENT_JOBS="$(ACT_CONCURRENT_JOBS)"'),
            3,
        )
        self.assertNotIn('docs/src/build.md, "Test GitHub Actions Locally"', makefile)


if __name__ == "__main__":
    unittest.main()
