#!/usr/bin/env python3
"""Regression checks for build-branch test discovery."""

from __future__ import annotations

import ast
from pathlib import Path
import unittest


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
