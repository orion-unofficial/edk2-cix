#!/usr/bin/env python3
"""Small shared helpers for build-branch test scripts."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path
from typing import Any


def run(
    cmd: list[str],
    cwd: Path,
    *,
    check: bool = True,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    full_env = os.environ.copy()
    if env:
        full_env.update(env)
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=check,
        env=full_env,
    )


def git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return run(["git", *args], repo, check=check)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_file(repo: Path, relative: str, text: str) -> None:
    path = repo / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def commit_all(repo: Path, message: str) -> str:
    git(repo, "add", ".", ":!.cache")
    git(repo, "commit", "-m", message)
    return rev_parse(repo, "HEAD")


def rev_parse(repo: Path, ref: str) -> str:
    return git(repo, "rev-parse", f"{ref}^{{commit}}").stdout.strip()


def show(repo: Path, ref: str, relative: str) -> str:
    return git(repo, "show", f"{ref}:{relative}").stdout


def switch_orphan(repo: Path, branch: str) -> None:
    git(repo, "switch", "--orphan", branch)
    for path in repo.iterdir():
        if path.name == ".git":
            continue
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()


def conflicted_scratch(op_dir: Path) -> Path:
    state = json.loads((op_dir / "state.json").read_text(encoding="utf-8"))
    for target in state["targets"]:
        if target.get("status") == "conflict":
            return Path(target["scratch"])
    raise AssertionError("operation has no conflicted scratch tree")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_function_tests(module_globals: dict[str, Any]) -> unittest.TestSuite:
    """Expose script-style tests to unittest discovery.

    Build-branch tests predate unittest discovery and are also useful as direct
    scripts. This keeps the direct script entry points intact while letting
    `python3 -m unittest discover -s scripts -p 'test_*.py'` find the same
    checks.
    """

    suite = unittest.TestSuite()
    module_name = module_globals.get("__name__")
    discovered = False

    for name, value in module_globals.items():
        if not name.startswith("test_") or not callable(value):
            continue
        if getattr(value, "__module__", None) != module_name:
            continue
        suite.addTest(unittest.FunctionTestCase(value, description=name))
        discovered = True

    main = module_globals.get("main")
    if not discovered and callable(main):
        suite.addTest(unittest.FunctionTestCase(main, description="main"))

    return suite
