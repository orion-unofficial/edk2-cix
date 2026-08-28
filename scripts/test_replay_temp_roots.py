#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


def load_script(name: str):
    path = REPO_ROOT / "src" / "scripts" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


REPLAY = load_script("replay_o6_release")
ROUNDTRIP = load_script("validate_fiptool_roundtrip")


class ReplayTempRootTests(unittest.TestCase):
    def test_explicit_output_creates_missing_temp_root(self) -> None:
        for name, module in (
            ("replay-extract", REPLAY),
            ("fiptool-roundtrip", ROUNDTRIP),
        ):
            with self.subTest(helper=name), tempfile.TemporaryDirectory() as tempdir:
                root = pathlib.Path(tempdir)
                input_path = root / "missing.bin"
                temp_root = root / "missing" / name
                args = mock.Mock(
                    input_path=str(input_path),
                    board="O6",
                    output_dir=str(root / "output"),
                    keep_workdir=False,
                )
                workdir = mock.Mock()
                workdir.name = str(temp_root / "work")

                with (
                    mock.patch.object(module, "DEFAULT_TMP_ROOT", temp_root),
                    mock.patch.object(module, "parse_args", return_value=args),
                    mock.patch.object(
                        module.tempfile, "TemporaryDirectory", return_value=workdir
                    ) as temporary_directory,
                    self.assertRaises(FileNotFoundError) as raised,
                ):
                    module.main()

                self.assertTrue(temp_root.is_dir())
                self.assertEqual(
                    temporary_directory.call_args.kwargs["dir"], str(temp_root)
                )
                self.assertEqual(raised.exception.args, (input_path,))


if __name__ == "__main__":
    unittest.main()
