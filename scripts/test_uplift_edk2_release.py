#!/usr/bin/env python3
"""Regression tests for the EDK2 stable-release orchestrator."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from reconstruction_common import ReconstructionError
import uplift_edk2_release


REPO_ROOT = Path(__file__).resolve().parents[1]


def write_policy(repo: Path) -> None:
    path = repo / "config" / "policies.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(
            {
                "unofficial_source_policy": {
                    "default_line": "1.3",
                    "lines": {
                        "1.2": {
                            "current_cix_release": "1.1",
                            "current_edk2_release": "202505",
                            "current_radxa_release": "1.2.4",
                            "current_ref": "source/unofficial/1.2/current",
                        },
                        "1.3": {
                            "current_cix_release": "1.2",
                            "current_edk2_release": "202605",
                            "current_radxa_release": "1.3.1",
                            "current_ref": "source/unofficial/1.3/current",
                        },
                    },
                }
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


class UpliftEdk2ReleaseTests(unittest.TestCase):
    def test_policy_defaults_follow_an_explicit_non_default_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            write_policy(repo)
            selected = uplift_edk2_release.policy_defaults(repo, "1.2")

        self.assertEqual(selected["line"], "1.2")
        self.assertEqual(selected["current_edk2_release"], "202505")
        self.assertEqual(selected["current_radxa_release"], "1.2.4")
        self.assertEqual(selected["current_ref"], "source/unofficial/1.2/current")

    def test_policy_defaults_reject_an_unknown_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            write_policy(repo)
            with self.assertRaisesRegex(ReconstructionError, "configured lines: 1.2, 1.3"):
                uplift_edk2_release.policy_defaults(repo, "1.4")

    def test_main_carries_all_defaults_from_the_requested_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            write_policy(repo)
            args = SimpleNamespace(
                edk2_base="edk2-stable202608",
                from_edk2_base="",
                radxa_release="",
                line="1.2",
                cix_release="",
                edk2_ref="",
                edk2_platforms_ref="",
                edk2_non_osi_ref="",
                compatibility_ref="",
                radxa_ref="",
                unofficial_ref="",
                from_ref="",
                release="",
                skip_render="0",
                verify="1",
                write="0",
                force="1",
                allow_replace="0",
                v="0",
            )
            parser = mock.Mock()
            parser.parse_args.return_value = args
            with (
                mock.patch.object(uplift_edk2_release, "parser", return_value=parser),
                mock.patch.object(uplift_edk2_release, "repo_root", return_value=repo),
                mock.patch.object(uplift_edk2_release, "check_immutable_refs") as check_refs,
                mock.patch.object(uplift_edk2_release, "maybe_integrate_upstream"),
                mock.patch.object(uplift_edk2_release, "maybe_promote_compatibility"),
                mock.patch.object(uplift_edk2_release, "maybe_integrate_radxa") as port_radxa,
                mock.patch.object(uplift_edk2_release, "maybe_promote_unofficial") as promote,
            ):
                uplift_edk2_release.main()

        check_refs.assert_called_once_with(repo)
        port_radxa.assert_called_once_with(
            repo,
            edk2_base="edk2-stable202608",
            from_edk2_base="edk2-stable202505",
            radxa_release="1.2.4",
            radxa_ref="",
            write="0",
            allow_replace="0",
            verbose=False,
        )
        promote.assert_called_once_with(
            repo,
            edk2_base="edk2-stable202608",
            from_edk2_base="edk2-stable202505",
            radxa_release="1.2.4",
            line="1.2",
            cix_release="1.1",
            from_ref="source/unofficial/1.2/current",
            unofficial_ref="",
            write="0",
            allow_replace="0",
            verbose=False,
        )

    def test_make_target_defaults_force_to_one_but_honours_override(self) -> None:
        base = [
            "make",
            "-n",
            "uplift-edk2-release",
            "EDK2_BASE=edk2-stable209912",
        ]
        default = subprocess.run(
            base,
            cwd=REPO_ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
        overridden = subprocess.run(
            [*base, "FORCE=0"],
            cwd=REPO_ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
        self.assertIn('FORCE="1"', default.stdout)
        self.assertIn('FORCE="0"', overridden.stdout)


if __name__ == "__main__":
    unittest.main()
