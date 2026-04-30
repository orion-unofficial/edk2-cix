#!/usr/bin/env python3

from __future__ import annotations

from contextlib import contextmanager
import pathlib
import subprocess
import tempfile
from collections.abc import Iterator, Sequence


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_VENDOR_REFS = ("source/release/upstream/edk2-202208/radxa-1.2.1", "main")


def git_path_exists(ref: str, repo_relpath: str) -> bool:
    result = subprocess.run(
        ["git", "cat-file", "-e", f"{ref}:{repo_relpath}"],
        cwd=REPO_ROOT,
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    return result.returncode == 0


def choose_vendor_ref(repo_relpath: str, refs: Sequence[str] = DEFAULT_VENDOR_REFS) -> str:
    for ref in refs:
        if git_path_exists(ref, repo_relpath):
            return ref
    raise FileNotFoundError(
        f"could not find {repo_relpath} in any candidate ref: {', '.join(refs)}"
    )


@contextmanager
def resolve_vendor_tool(
    *,
    explicit_path: pathlib.Path | None,
    repo_relpath: str,
    refs: Sequence[str] = DEFAULT_VENDOR_REFS,
) -> Iterator[pathlib.Path]:
    if explicit_path is not None:
        yield explicit_path.resolve()
        return

    ref = choose_vendor_ref(repo_relpath, refs=refs)
    filename = pathlib.Path(repo_relpath).name
    with tempfile.TemporaryDirectory(prefix=f"{filename}-vendor.") as td:
        materialized = pathlib.Path(td) / filename
        with materialized.open("wb") as handle:
            subprocess.run(
                ["git", "show", f"{ref}:{repo_relpath}"],
                cwd=REPO_ROOT,
                check=True,
                stdout=handle,
            )
        materialized.chmod(0o755)
        yield materialized
