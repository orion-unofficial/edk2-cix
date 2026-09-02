#!/usr/bin/env python3
"""Regression checks for event-driven qualification and branch boundaries."""

from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
WORKFLOW_ROOT = REPO_ROOT / ".github" / "workflows"


class QualificationPolicyTests(unittest.TestCase):
    def workflow(self, name: str) -> str:
        return (WORKFLOW_ROOT / name).read_text(encoding="utf-8")

    def test_only_dependency_monitoring_is_scheduled(self) -> None:
        scheduled = [
            path.name
            for path in sorted(WORKFLOW_ROOT.glob("*.yaml"))
            if "\n  schedule:\n" in path.read_text(encoding="utf-8")
        ]

        self.assertEqual(scheduled, ["upstream-versions.yaml"])

    def test_build_tip_runs_complete_firmware_qualification(self) -> None:
        text = self.workflow("build-branch-ci.yaml")

        triggers = (
            "merge_group:",
            "pull_request:",
            "push:",
            "workflow_dispatch:",
        )
        for trigger in triggers:
            self.assertIn(trigger, text)
        self.assertIn("documentation/licensing-only change", text)
        self.assertIn("uses: ./.github/workflows/secure-boot-audit.yaml", text)
        self.assertIn(
            "uses: ./.github/workflows/deterministic-replay.yaml",
            text,
        )
        self.assertIn("name: Qualification summary", text)
        self.assertIn('if: ${{ always() }}', text)
        self.assertIn("CURRENT_SOURCE_RESULT", text)
        self.assertIn("UPSTREAM_REPLAY_RESULT", text)

    def test_current_and_replay_matrices_cover_required_products(self) -> None:
        current = self.workflow("secure-boot-audit.yaml")
        replay = self.workflow("deterministic-replay.yaml")

        self.assertIn("workflow_call:", current)
        self.assertIn("workflow_call:", replay)
        self.assertEqual(current.count("board: O6\n"), 2)
        self.assertEqual(current.count("board: O6N\n"), 2)
        self.assertIn("firmware_fixes: false", current)
        self.assertIn("firmware_fixes: true", current)
        self.assertIn("ARTEFACT_MODE=custom", current)
        self.assertIn("CIX_RELEASE=v1.2", current)
        self.assertIn("edk2-202208/radxa-1.3.1", replay)
        self.assertIn(
            "REPLAY_VERSION: ${{ inputs.replay_version || '1.3.1' }}",
            replay,
        )

    def test_published_qualification_runs_are_not_cancelled(self) -> None:
        build = self.workflow("build-branch-ci.yaml")
        versions = self.workflow("upstream-versions.yaml")

        for text in (build, versions):
            self.assertIn(
                "cancel-in-progress: "
                "${{ github.event_name == 'pull_request' }}",
                text,
            )
            self.assertIn("github.run_id", text)

        self.assertIn(
            "cancel-in-progress: false",
            self.workflow("manual-firmware-build.yaml"),
        )
        for name in (
            "deterministic-replay.yaml",
            "secure-boot-audit.yaml",
        ):
            text = self.workflow(name)
            self.assertIn("workflow_call:", text)
            self.assertNotIn("\nconcurrency:\n", text)

    def test_docs_deploy_only_from_a_build_push(self) -> None:
        text = self.workflow("build-docs.yaml")

        self.assertIn("workflow_call:", text)
        self.assertIn(
            "github.event_name == 'push' && github.ref == 'refs/heads/build'",
            text,
        )

    def test_branch_content_boundary_is_explicit(self) -> None:
        readme = (REPO_ROOT / "README.md").read_text(encoding="utf-8")
        maintenance = (REPO_ROOT / "MAINTENANCE.md").read_text(
            encoding="utf-8"
        )
        summary = (REPO_ROOT / "docs" / "src" / "SUMMARY.md").read_text(
            encoding="utf-8"
        )
        extended = (
            REPO_ROOT / "docs" / "src" / "maintenance-and-ci.md"
        ).read_text(encoding="utf-8")
        test_workflow = self.workflow("test-branch-ci.yaml")

        self.assertIn("## Repository maintenance", readme)
        self.assertIn("[`MAINTENANCE.md`](MAINTENANCE.md)", readme)
        self.assertNotIn("`test` branch", readme)
        self.assertNotIn("## How does CI work?", readme)
        self.assertIn("# Repository Maintenance", maintenance)
        self.assertIn("[Maintenance and CI](maintenance-and-ci.md)", summary)
        self.assertIn("root `MAINTENANCE.md`", extended)
        self.assertIn("exists on `test`", extended)
        self.assertIn("branches:\n      - test", test_workflow)
        self.assertIn(
            "uses: ./.github/workflows/build-docs.yaml",
            test_workflow,
        )
        self.assertNotIn("\n  schedule:\n", test_workflow)


if __name__ == "__main__":
    unittest.main()
