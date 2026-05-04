#!/usr/bin/env python3
"""Verify persistent source refs do not depend on generated cache refs."""

from __future__ import annotations

import argparse
import os
import time
from pathlib import Path

from reconstruction_common import ReconstructionError, for_each_ref, format_duration, git, main_wrapper, repo_root, resolve_ref, truthy


HELP = """verify-ref-integrity

No variables are required.

Optional variables:
  V=0|1  Print each scanned persistent source ref.

Checks:
  - persistent source refs must not refer to generated source/cache refs

Generated refs are cache artefacts. They may be mentioned by build-branch
manifests and reconstruction tooling, but persistent buildable source branches
must not rely on them being present in a pruned clone.
"""

GENERATED_REF_PATTERN = r"source/cache/"

SCAN_PATHS = (
    ".github",
    "Makefile",
    "README.md",
    "book.toml",
    "docs",
    "scripts",
    "src/Makefile",
    "src/scripts",
    "src/tools",
    "validation",
)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def scan_ref(repo: Path, ref: str) -> list[str]:
    result = git(
        repo,
        "grep",
        "-I",
        "--line-number",
        "--extended-regexp",
        GENERATED_REF_PATTERN,
        resolve_ref(repo, ref),
        "--",
        *SCAN_PATHS,
        check=False,
    )
    if result.returncode == 1:
        return []
    if result.returncode != 0:
        raise ReconstructionError((result.stderr or result.stdout).strip())
    return [line for line in result.stdout.splitlines() if line]


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    refs = (
        for_each_ref(repo, "source/unofficial")
        + for_each_ref(repo, "source/vendor/radxa")
        + for_each_ref(repo, "source/port/radxa")
    )

    problems: list[str] = []
    for ref in refs:
        if verbose:
            print(f"scanning persistent source ref: {ref}")
        matches = scan_ref(repo, ref)
        if matches:
            problems.append(f"{ref} references generated cache refs:\n" + "\n".join(f"  - {line}" for line in matches))

    if problems:
        raise ReconstructionError("persistent source reference integrity failed:\n" + "\n\n".join(problems))

    print(
        "validated persistent source reference integrity: "
        f"{len(refs)} persistent source refs "
        f"in {format_duration(time.monotonic() - started)}"
    )


if __name__ == "__main__":
    main_wrapper(main)
