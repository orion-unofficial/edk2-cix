#!/usr/bin/env python3
"""Scan generated control files and selected commits for identity/path hygiene issues."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path

from reconstruction_common import ReconstructionError, git, main_wrapper, repo_root, truthy


HELP = """check-identity-hygiene

Optional variables:
  SCAN_COMMITS=1  Also scan commits reachable from HEAD for generated-identity strings.
  V=1             Print scanned paths.

The scanner is intentionally conservative for the control branch. It looks for
host-specific paths, generated assistant identity strings, and embedded personal
email addresses in generated scripts, manifests, and documentation.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--scan-commits", default=os.environ.get("SCAN_COMMITS", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def suspicious_patterns() -> list[tuple[str, re.Pattern[str]]]:
    private_tmp = "/" + "private/tmp"
    generic_tmp = "/" + "tmp/"
    users_path = "/" + "Users/"
    generated_name = "co" + "dex"
    return [
        ("host path", re.compile(re.escape(private_tmp) + "|" + re.escape(generic_tmp) + "|" + re.escape(users_path))),
        ("generated assistant identity", re.compile(generated_name, re.IGNORECASE)),
        ("embedded email", re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}")),
    ]


def tracked_files(repo: Path) -> list[str]:
    result = git(repo, "ls-files", "-z")
    return [p for p in result.stdout.split("\0") if p]


def scan_files(repo: Path, verbose: bool) -> list[str]:
    problems: list[str] = []
    patterns = suspicious_patterns()
    for rel in tracked_files(repo):
        path = repo / rel
        if not path.is_file():
            continue
        if verbose:
            print(f"scan {rel}")
        data = path.read_bytes()
        if b"\0" in data:
            continue
        text = data.decode("utf-8", errors="ignore")
        for label, pattern in patterns:
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                problems.append(f"{rel}:{line}: {label}: {match.group(0)}")
    return problems


def scan_commits(repo: Path) -> list[str]:
    generated_name = "co" + "dex"
    result = git(repo, "log", "--format=%H%x00%an%x00%ae%x00%cn%x00%ce%x00%s")
    problems: list[str] = []
    for line in result.stdout.splitlines():
        parts = line.split("\0")
        if len(parts) != 6:
            continue
        commit, author, author_email, committer, committer_email, subject = parts
        haystack = " ".join([author, author_email, committer, committer_email, subject])
        if generated_name.lower() in haystack.lower():
            problems.append(f"{commit}: generated identity string in commit metadata")
    return problems


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    problems = scan_files(repo, truthy(args.v))
    if truthy(args.scan_commits):
        problems.extend(scan_commits(repo))
    if problems:
        raise ReconstructionError("identity hygiene check failed:\n" + "\n".join(f"  - {p}" for p in problems))
    print("identity hygiene check passed")


if __name__ == "__main__":
    main_wrapper(main)
