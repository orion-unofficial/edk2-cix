#!/usr/bin/env python3
"""Validate that a minimised exported repo can reconstruct supported sources."""

from __future__ import annotations

import argparse
import os
import sys
import tempfile
import time
from pathlib import Path

from reconstruction_common import ReconstructionError, cache_dir, default_release, format_duration, git, main_wrapper, repo_root, run, temp_dir, truthy


HELP = """verify-minimised-clone

Optional variables:
  DIR=<path>     Directory to use for the verification workspace. If unset,
                 a temporary directory under .cache/edk2-cix/tmp is used and
                 removed automatically.
  KEEP=0|1      Keep the verification workspace after completion. Default: 0.
  REPACK=0|1    Repack the exported bare repo. Default: 1.
  V=0|1         Print delegated git and make operations.

The check exports a minimised bare repository, clones it normally, verifies the
source/build matrix from that clone, and renders the default firmware source target.
It fails if the export contains generated source/cache/** branches.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--dir", default=os.environ.get("DIR", ""))
    p.add_argument("--keep", default=os.environ.get("KEEP", "0"))
    p.add_argument("--repack", default=os.environ.get("REPACK", "1"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def ensure_empty_dir(path: Path) -> None:
    if path.exists() and any(path.iterdir()):
        raise ReconstructionError(f"verification workspace already exists and is not empty: {path}")
    path.mkdir(parents=True, exist_ok=True)


def run_step(label: str, cmd: list[str], verbose: bool) -> None:
    started = time.monotonic()
    if verbose:
        print("+ " + " ".join(cmd), file=sys.stderr)
    run(cmd, capture=not verbose)
    print(f"[verify-minimised] {label} completed in {format_duration(time.monotonic() - started)}", file=sys.stderr)


def require_default_branch(bare: Path, checkout: Path | None = None) -> None:
    bare_head = run(["git", "--git-dir", str(bare), "symbolic-ref", "--quiet", "HEAD"], check=False)
    if bare_head.returncode != 0 or bare_head.stdout.strip() != "refs/heads/build":
        detail = bare_head.stdout.strip() or bare_head.stderr.strip() or "HEAD is not symbolic"
        raise ReconstructionError(f"minimised export default branch is not build: {detail}")
    if checkout:
        checkout_head = git(checkout, "symbolic-ref", "--quiet", "--short", "HEAD", check=False)
        if checkout_head.returncode != 0 or checkout_head.stdout.strip() != "build":
            detail = checkout_head.stdout.strip() or checkout_head.stderr.strip() or "checkout is detached"
            raise ReconstructionError(f"normal clone of minimised export did not check out build: {detail}")


def verify_from_workspace(repo: Path, workspace: Path, keep: bool, repack: str, verbose: bool) -> None:
    bare = workspace / "minimal.git"
    checkout = workspace / "checkout"
    default_variant = default_release(repo)

    print(f"[verify-minimised] Exporting minimised clone to {bare}", file=sys.stderr)
    run_step(
        "Export",
        [
            sys.executable,
            str(repo / "scripts" / "create_minimised_clone.py"),
            "--dir",
            str(bare),
            "--repack",
            repack,
            "--v",
            "1" if verbose else "0",
        ],
        verbose,
    )

    cache_refs = git(bare, "for-each-ref", "--format=%(refname)", "refs/heads/source/cache", check=False)
    if cache_refs.returncode == 0 and cache_refs.stdout.strip():
        raise ReconstructionError("minimised export unexpectedly contains source/cache/** refs")
    require_default_branch(bare)

    print(f"[verify-minimised] Cloning exported repo to {checkout}", file=sys.stderr)
    run_step("Clone", ["git", "clone", str(bare), str(checkout)], verbose)
    require_default_branch(bare, checkout)

    print("[verify-minimised] Verifying source matrix from minimised clone", file=sys.stderr)
    run_step(
        "Verification",
        [
            "make",
            "-C",
            str(checkout),
            "verify-build-matrix",
            "verify-manifest-integrity",
            "check-ref-integrity",
            "check-help-cache",
            "ref-report",
            "--no-print-directory",
        ],
        verbose,
    )

    print(f"[verify-minimised] Rendering default source target: {default_variant}", file=sys.stderr)
    run_step(
        "Render",
        [
            "make",
            "-C",
            str(checkout),
            "render-release-branch",
            f"RELEASE={default_variant}",
            "--no-print-directory",
        ],
        verbose,
    )

    if keep:
        print(f"[verify-minimised] Kept verification workspace: {workspace}", file=sys.stderr)


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    keep = truthy(args.keep)
    started = time.monotonic()

    if args.dir:
        workspace = Path(args.dir).expanduser().resolve()
        ensure_empty_dir(workspace)
        verify_from_workspace(repo, workspace, keep=True, repack=args.repack, verbose=verbose)
    elif keep:
        workspace = Path(tempfile.mkdtemp(prefix="minimised-verify-", dir=cache_dir(repo, "tmp")))
        verify_from_workspace(repo, workspace, keep=True, repack=args.repack, verbose=verbose)
    else:
        with temp_dir(repo, "minimised-verify-") as tmp:
            verify_from_workspace(repo, Path(tmp), keep=False, repack=args.repack, verbose=verbose)

    print(f"validated minimised clone export in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
