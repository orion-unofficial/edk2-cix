#!/usr/bin/env python3

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from promote_unofficial_release import checkpoint_record
from test_support import commit_all, git, load_json, write_file


class PromoteUnofficialReleaseTests(unittest.TestCase):
    def test_checkpoint_correction_preserves_original_predecessor(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "-q")
            git(repo, "config", "user.name", "Checkpoint Test")
            git(repo, "config", "user.email", "checkpoint-test")
            write_file(repo, "firmware.txt", "old\n")
            old = commit_all(repo, "old")
            write_file(repo, "firmware.txt", "corrected\n")
            corrected = commit_all(repo, "corrected")

            target = "source/unofficial/1.3.1/edk2-stable202608"
            manifest = repo / "config" / "refs-unofficial.json"
            manifest.parent.mkdir()
            manifest.write_text(
                json.dumps(
                    {
                        "refs": [
                            {
                                "ref": target,
                                "previous_unofficial_ref": "source/unofficial/1.3.1/edk2-stable202605",
                                "previous_unofficial_object_id": old,
                            }
                        ]
                    }
                )
                + "\n",
                encoding="utf-8",
            )

            with patch(
                "promote_unofficial_release.radxa_source_ref",
                return_value="source/port/radxa/1.3.1/edk2-stable202608",
            ):
                checkpoint_record(
                    repo,
                    ref=target,
                    source_oid=corrected,
                    line="1.3",
                    radxa_release="1.3.1",
                    edk2_base="edk2-stable202608",
                    previous_ref="source/unofficial/1.3/current",
                    previous_object_id=corrected,
                )

            record = load_json(manifest)["refs"][0]
            self.assertEqual(
                record["previous_unofficial_ref"],
                "source/unofficial/1.3.1/edk2-stable202605",
            )
            self.assertEqual(record["previous_unofficial_object_id"], old)
            self.assertEqual(record["object_id"], corrected)


if __name__ == "__main__":
    unittest.main()
