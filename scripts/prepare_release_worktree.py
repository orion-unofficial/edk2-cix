#!/usr/bin/env python3
"""Prepare transient inputs inside a rendered release worktree."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
from pathlib import Path

from reconstruction_common import ReconstructionError, main_wrapper, truthy


CERT_NAMES = ("trusted_key_no.crt", "nt_fw_cert.crt", "nt_fw_key.crt")


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--worktree", type=Path, required=True)
    p.add_argument("--signing-cert-source-dir", default=os.environ.get("SIGNING_CERT_SOURCE_DIR", ""))
    p.add_argument("--print-make-arg", action="store_true")
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def resolve_source(path_text: str, cwd: Path) -> Path:
    path = Path(path_text).expanduser()
    if not path.is_absolute():
        path = cwd / path
    return path.resolve()


def copy_signing_certs(worktree: Path, source: Path, verbose: bool) -> Path:
    if not source.is_dir():
        raise ReconstructionError(f"SIGNING_CERT_SOURCE_DIR does not exist or is not a directory: {source}")

    missing = [name for name in CERT_NAMES if not (source / name).is_file()]
    if missing:
        raise ReconstructionError(
            "SIGNING_CERT_SOURCE_DIR is missing required certificate file(s): " + ", ".join(missing)
        )

    digest = hashlib.sha256(str(source).encode("utf-8")).hexdigest()[:16]
    target = worktree / ".cache" / "edk2-cix" / "signing-certs" / digest
    target.mkdir(parents=True, exist_ok=True)
    for name in CERT_NAMES:
        shutil.copy2(source / name, target / name)
    if verbose:
        print(f"Staged signing certificate inputs under {target}", file=sys.stderr)
    return target


def main() -> None:
    args = parser().parse_args()
    if not args.signing_cert_source_dir:
        return

    worktree = args.worktree.resolve()
    target = copy_signing_certs(
        worktree,
        resolve_source(args.signing_cert_source_dir, Path.cwd()),
        truthy(args.v),
    )
    try:
        relative = target.relative_to(worktree)
    except ValueError:
        relative = target

    if args.print_make_arg:
        print(f"SIGNING_CERT_SOURCE_DIR={relative}")


if __name__ == "__main__":
    main_wrapper(main)
