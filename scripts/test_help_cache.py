#!/usr/bin/env python3
"""Regression tests for runtime help-cache behaviour."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path


def repo_root() -> Path:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return Path(result.stdout.strip())


def run(repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", *args],
        cwd=repo,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def main() -> None:
    repo = repo_root()
    committed_cache = repo / "config" / "help-cache.json"
    if committed_cache.exists():
        raise SystemExit("config/help-cache.json must not be committed")

    runtime_cache = repo / ".cache" / "edk2-cix" / "help" / "help-cache.json"
    runtime_cache.unlink(missing_ok=True)

    first = run(repo, "scripts/help_cache.py", "--print-default-release")
    if "Updating runtime help cache" not in first.stderr:
        raise SystemExit("cold help-cache generation did not report progress")
    if not first.stdout.strip().startswith("edk2-"):
        raise SystemExit(f"unexpected default source target: {first.stdout.strip()}")
    with runtime_cache.open("r", encoding="utf-8") as f:
        json.load(f)

    second = run(repo, "scripts/help_cache.py", "--print-default-release")
    if "Updating runtime help cache" in second.stderr:
        raise SystemExit("warm help-cache lookup unexpectedly refreshed cache")
    if second.stdout != first.stdout:
        raise SystemExit("warm help-cache lookup changed default source target")


if __name__ == "__main__":
    main()
