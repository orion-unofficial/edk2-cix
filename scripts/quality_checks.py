#!/usr/bin/env python3
"""Run build-branch tests and lint checks inside the quality container."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str], *, stdout=None) -> None:
    print("+ " + " ".join(cmd), file=sys.stderr)
    subprocess.run(cmd, check=True, stdout=stdout)


def git_files(*patterns: str) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", *patterns],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return [line for line in result.stdout.splitlines() if line and Path(line).exists()]


def lint_json() -> None:
    files = git_files("config/*.json")
    for file_name in files:
        with Path(file_name).open("r", encoding="utf-8") as f:
            json.load(f)
        run(["jq", "--exit-status", "type", file_name], stdout=subprocess.DEVNULL)


def lint_yaml() -> None:
    files = git_files("*.yaml", "*.yml", ".github/**/*.yaml", ".github/**/*.yml")
    if files:
        run(["yamllint", *sorted(set(files))])


def lint_python() -> None:
    run(["flake8", "--extend-ignore=E203,E501,W503", "scripts"])


def lint_shell() -> None:
    files = git_files("*.sh", "scripts/*.sh")
    if files:
        run(["shellcheck", *files])


def lint_markdown() -> None:
    files = git_files("*.md", "config/**/*.md", "scripts/**/*.md")
    if files:
        run(["mdl", "--rules", "~MD013,~MD026,~MD029", *files])


def lint() -> None:
    lint_json()
    lint_yaml()
    lint_python()
    lint_shell()
    lint_markdown()


def test() -> None:
    run(["python3", "-m", "py_compile", *git_files("scripts/*.py")])
    run(["python3", "scripts/test_check_source_freshness.py"])
    run(["make", "verify-build-matrix", "--no-print-directory"])
    run(["make", "verify-manifest-integrity", "--no-print-directory"])
    run(["make", "verify-ref-integrity", "--no-print-directory"])
    run(["make", "verify-minimised-clone", "REPACK=0", "--no-print-directory"])
    run(["make", "check-vendor-workflow-drift", "--no-print-directory"])
    run(["make", "ref-report", "--no-print-directory"], stdout=subprocess.DEVNULL)
    run(["make", "cleanup-report", "--no-print-directory"], stdout=subprocess.DEVNULL)
    run(["make", "prune", "--no-print-directory"], stdout=subprocess.DEVNULL)
    run(["make", "check-identity-integrity", "SCAN_SOURCE_REFS=1", "--no-print-directory"])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("test", "lint", "all"))
    args = parser.parse_args()

    if args.mode in {"test", "all"}:
        test()
    if args.mode in {"lint", "all"}:
        lint()


if __name__ == "__main__":
    main()
