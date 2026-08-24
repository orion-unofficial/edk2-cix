#!/usr/bin/env python3
"""Run build-branch tests and lint checks.

The test mode is the self-contained build-branch quality gate. It intentionally
does not include remote freshness checks such as check-upstream-versions, docs
builds, firmware build/replay validation, or every parameterised
verify-release-branch invocation.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

from reconstruction_common import format_duration


REPO_ROOT = Path(__file__).resolve().parents[1]


def run(cmd: list[str], *, stdout=None) -> None:
    print("+ " + " ".join(cmd), file=sys.stderr)
    started = time.monotonic()
    process = subprocess.Popen(cmd, stdout=stdout)
    while True:
        try:
            returncode = process.wait(timeout=30)
            break
        except subprocess.TimeoutExpired:
            print(
                f"[quality] Command still running "
                f"({format_duration(time.monotonic() - started)}): {' '.join(cmd)}",
                file=sys.stderr,
            )
    if returncode != 0:
        print(
            f"command failed with exit status {returncode}: {' '.join(cmd)}",
            file=sys.stderr,
        )
        raise SystemExit(returncode)


def git_files(*patterns: str) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", *patterns],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return [line for line in result.stdout.splitlines() if line and Path(line).exists()]


def lint_json() -> None:
    files = git_files("config/*.json", "docs/devenv.lock")
    for file_name in files:
        with Path(file_name).open("r", encoding="utf-8") as f:
            json.load(f)
        run(["jq", "--exit-status", "type", file_name], stdout=subprocess.DEVNULL)


def lint_yaml() -> None:
    files = git_files("*.yaml", "*.yml", ".github/**/*.yaml", ".github/**/*.yml", "docs/**/*.yaml", "docs/**/*.yml")
    if files:
        run(["yamllint", *sorted(set(files))])


def lint_python() -> None:
    run(["flake8", "--extend-ignore=E203,E501,W503", "scripts", "docs/scripts"])


def lint_shell() -> None:
    files = git_files("*.sh", "scripts/*.sh", "docs/scripts/*.sh")
    if files:
        run(["shellcheck", *files])


def lint_markdown() -> None:
    files = git_files("*.md", "config/**/*.md", "scripts/**/*.md", "docs/**/*.md")
    if files:
        run(["mdl", "--rules", "~MD013,~MD026,~MD029", *files])


def lint() -> None:
    lint_json()
    lint_yaml()
    lint_python()
    lint_shell()
    lint_markdown()


def test(*, skip_minimised_clone: bool = False) -> None:
    run(["python3", "-m", "py_compile", *git_files("scripts/*.py")])
    run(["python3", "-m", "unittest", "discover", "-s", "scripts", "-p", "test_*.py"])
    run(["make", "verify-build-matrix", "--no-print-directory"])
    run(["make", "verify-manifest-integrity", "--no-print-directory"])
    run(["make", "verify-source-policy", "--no-print-directory"])
    run(["make", "verify-source-lifecycle", "--no-print-directory"])
    run(["make", "check-ref-integrity", "--no-print-directory"])
    run(["make", "check-source-metadata", "--no-print-directory"])
    run(["make", "check-help-cache", "--no-print-directory"])
    run(["make", "check-first-output-latency", "--no-print-directory"])
    if not skip_minimised_clone:
        run(["make", "verify-minimised-clone", "REPACK=0", "--no-print-directory"])
    run(["make", "check-vendor-workflow-drift", "--no-print-directory"])
    run(["make", "ref-report", "--no-print-directory"], stdout=subprocess.DEVNULL)
    run(["make", "cleanup-report", "--no-print-directory"], stdout=subprocess.DEVNULL)
    run(["make", "prune", "--no-print-directory"], stdout=subprocess.DEVNULL)
    run(["make", "verify-identity-integrity", "--no-print-directory"])


def main() -> None:
    os.environ.setdefault("PYTHONPYCACHEPREFIX", str(REPO_ROOT / ".cache" / "edk2-cix" / "pycache"))

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("test", "lint", "all"))
    parser.add_argument(
        "--skip-minimised-clone",
        action="store_true",
        help="Skip the recursive minimised export check when testing an exported clone.",
    )
    args = parser.parse_args()

    if args.mode in {"test", "all"}:
        test(skip_minimised_clone=args.skip_minimised_clone)
    if args.mode in {"lint", "all"}:
        lint()


if __name__ == "__main__":
    main()
