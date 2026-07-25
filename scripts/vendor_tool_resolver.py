#!/usr/bin/env python3

from __future__ import annotations

from contextlib import contextmanager
import pathlib
import subprocess
import tempfile
from collections.abc import Iterator, Sequence


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_VENDOR_REFS = ("source/delta/radxa/1.2.1/edk2-stable202208", "main")


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


def delta_patch_contains(ref: str, repo_relpath: str) -> bool:
    result = subprocess.run(
        ["git", "show", f"{ref}:delta.patch"],
        cwd=REPO_ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        errors="ignore",
    )
    if result.returncode != 0:
        return False
    marker = f"diff --git a/{repo_relpath} b/{repo_relpath}"
    return marker in result.stdout


def source_has_path(ref: str, repo_relpath: str) -> bool:
    return git_path_exists(ref, repo_relpath) or delta_patch_contains(ref, repo_relpath)


def choose_vendor_ref(repo_relpath: str, refs: Sequence[str] = DEFAULT_VENDOR_REFS) -> str:
    for ref in refs:
        if source_has_path(ref, repo_relpath):
            return ref
    raise FileNotFoundError(
        f"could not find {repo_relpath} in any candidate ref: {', '.join(refs)}"
    )


def materialise_from_delta(ref: str, repo_relpath: str, output_path: pathlib.Path) -> None:
    patch = subprocess.run(
        ["git", "show", f"{ref}:delta.patch"],
        cwd=REPO_ROOT,
        check=True,
        stdout=subprocess.PIPE,
    ).stdout
    subprocess.run(
        ["git", "apply", "--binary", "--include", repo_relpath, "-"],
        cwd=output_path,
        input=patch,
        check=True,
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
        td_path = pathlib.Path(td)
        if git_path_exists(ref, repo_relpath):
            materialised = td_path / filename
            with materialised.open("wb") as handle:
                subprocess.run(
                    ["git", "show", f"{ref}:{repo_relpath}"],
                    cwd=REPO_ROOT,
                    check=True,
                    stdout=handle,
                )
        else:
            materialise_from_delta(ref, repo_relpath, td_path)
            materialised = td_path / repo_relpath
        materialised.chmod(0o755)
        yield materialised
