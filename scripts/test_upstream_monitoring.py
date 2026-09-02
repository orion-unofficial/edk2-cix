#!/usr/bin/env python3
"""Regression checks for complete and date-coupled upstream monitoring."""

import json
from pathlib import Path
import tempfile
import unittest

from check_upstream_versions import (
    RemoteRef,
    remote_selected_at_or_before,
    validate_remote_monitoring,
)
from reconstruction_common import ReconstructionError


class UpstreamMonitoringTests(unittest.TestCase):
    def write_json(self, path: Path, payload: dict) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload), encoding="utf-8")

    def test_source_remote_requires_a_check_or_reasoned_opt_out(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="upstream-monitoring-test-"
        ) as tmp:
            root = Path(tmp)
            remotes_path = root / "config" / "remotes.json"
            self.write_json(
                remotes_path,
                {
                    "remotes": {
                        "source": {
                            "type": "upstream",
                            "url": "https://example.invalid/source",
                        }
                    }
                },
            )
            self.write_json(
                root / "config" / "refs-source.json",
                {
                    "refs": [
                        {
                            "ref": "source/base/example/v1",
                            "remote": "source",
                        }
                    ]
                },
            )

            with self.assertRaisesRegex(
                ReconstructionError,
                "source remotes lack",
            ):
                validate_remote_monitoring(root, [])

            self.write_json(
                remotes_path,
                {
                    "remotes": {
                        "source": {
                            "type": "upstream",
                            "monitor": False,
                        }
                    }
                },
            )
            with self.assertRaisesRegex(
                ReconstructionError,
                "source remotes lack",
            ):
                validate_remote_monitoring(root, [])

            self.write_json(
                remotes_path,
                {
                    "remotes": {
                        "source": {
                            "type": "upstream",
                            "monitor": False,
                            "monitor_reason": (
                                "upstream no longer publishes this "
                                "retained input"
                            ),
                        }
                    }
                },
            )
            validate_remote_monitoring(root, [])

    def test_date_coupled_snapshot_selects_the_expected_commit(self) -> None:
        timestamp = "2026-08-12T12:34:56Z"
        snapshot_ref = f"refs/heads/master@{{{timestamp}}}"
        selected = remote_selected_at_or_before(
            Path("."),
            "edk2-platforms",
            "refs/heads/master",
            timestamp,
            {"edk2-platforms": [RemoteRef("selected-object", snapshot_ref)]},
            False,
        )

        self.assertIsNotNone(selected)
        self.assertEqual(selected.object_id, "selected-object")
        self.assertEqual(selected.label, snapshot_ref)


if __name__ == "__main__":
    unittest.main()
