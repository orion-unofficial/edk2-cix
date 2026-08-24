#!/usr/bin/env python3
"""Regression checks for build-branch test discovery."""

from __future__ import annotations

import ast
from pathlib import Path
import unittest
from unittest.mock import patch

import quality_checks


SCRIPT_DIR = Path(__file__).resolve().parent


def has_unittest_testcase(tree: ast.Module) -> bool:
    for node in tree.body:
        if not isinstance(node, ast.ClassDef):
            continue
        for base in node.bases:
            if isinstance(base, ast.Attribute) and base.attr == "TestCase":
                return True
            if isinstance(base, ast.Name) and base.id == "TestCase":
                return True
    return False


class QualityCoverageTests(unittest.TestCase):
    def test_exported_clone_gate_avoids_recursive_export(self) -> None:
        with patch.object(quality_checks, "git_files", return_value=[]), patch.object(quality_checks, "run") as run:
            quality_checks.test(skip_minimised_clone=True)

        commands = [call.args[0] for call in run.call_args_list]
        self.assertFalse(any("verify-minimised-clone" in command for command in commands))
        self.assertTrue(any("verify-source-policy" in command for command in commands))
        self.assertTrue(any("verify-source-lifecycle" in command for command in commands))

    def test_normal_quality_gate_includes_minimised_export(self) -> None:
        with patch.object(quality_checks, "git_files", return_value=[]), patch.object(quality_checks, "run") as run:
            quality_checks.test()

        commands = [call.args[0] for call in run.call_args_list]
        self.assertTrue(any("verify-minimised-clone" in command for command in commands))

    def test_quality_container_mounts_shared_git_objects_read_only(self) -> None:
        runner = (SCRIPT_DIR / "run_quality_container.sh").read_text(encoding="utf-8")

        self.assertIn('git_objects="$(git -C "$repo" rev-parse', runner)
        self.assertIn('git_objects/info/alternates', runner)
        self.assertIn('--volume "$alternate:$alternate:ro"', runner)
        self.assertIn('--volume "$git_common:$git_common:ro"', runner)

    def test_script_style_tests_are_exposed_to_unittest_discovery(self) -> None:
        missing: list[str] = []

        for path in sorted(SCRIPT_DIR.glob("test_*.py")):
            if path.name in {"test_support.py", Path(__file__).name}:
                continue

            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            functions = [
                node.name
                for node in tree.body
                if isinstance(node, ast.FunctionDef)
            ]
            module_tests = [
                name for name in functions if name.startswith("test_")
            ]
            has_main = "main" in functions
            has_load_tests = "load_tests" in functions

            if has_unittest_testcase(tree):
                continue
            if (module_tests or has_main) and not has_load_tests:
                missing.append(path.name)

        self.assertEqual(
            missing,
            [],
            "script-style test files must define load_tests so unittest "
            f"discovery runs them: {', '.join(missing)}",
        )


if __name__ == "__main__":
    unittest.main()
