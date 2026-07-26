#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "src" / "scripts" / "replay_o6_release.py"
SPEC = importlib.util.spec_from_file_location("replay_o6_release", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)


class ReplayO6ReleaseTests(unittest.TestCase):
    def test_copies_published_nt_fw_for_exact_replay(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            root = pathlib.Path(tempdir)
            flash_dir = root / "flash"
            output_dir = root / "output"
            flash_dir.mkdir()
            (flash_dir / "nt-fw.bin").write_bytes(b"published-bl33")

            copied = REPLAY.copy_exact_replay_firmware(flash_dir, output_dir)

            self.assertEqual(copied, output_dir / "firmware" / "nt-fw.bin")
            self.assertEqual(copied.read_bytes(), b"published-bl33")

    def test_rebuild_wrapper_passes_exact_replay_nt_fw(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            root = pathlib.Path(tempdir)
            wrapper = root / "rebuild.sh"
            env_values = {
                "BUILD_DATE": "2026-03-19T00:00:00+00:00",
                "SOURCE_DATE_EPOCH": "1773878400",
                "PM_CONFIG_SOURCE_DATE_EPOCH": "1773878400",
                "SIGNING_CERT_SOURCE_DIR": str(root / "certs"),
                "EXACT_REPLAY_NT_FW_SOURCE": str(root / "firmware" / "nt-fw.bin"),
            }

            REPLAY.write_rebuild_wrapper(wrapper, env_values, ("firmware",))

            text = wrapper.read_text(encoding="utf-8")
            self.assertIn("ARTEFACT_MODE=upstream", text)
            self.assertIn("EXACT_REPLAY_NT_FW_SOURCE=", text)


if __name__ == "__main__":
    unittest.main()
