#!/usr/bin/env python3
"""Shared helpers for the EDK2-CIX reconstruction control branch."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Iterable


class ReconstructionError(RuntimeError):
    """Raised for expected user-facing workflow failures."""


def run(cmd: list[str], cwd: Path | str | None = None, check: bool = True, capture: bool = True) -> subprocess.CompletedProcess[str]:
    kwargs: dict[str, Any] = {"text": True}
    if capture:
        kwargs.update({"stdout": subprocess.PIPE, "stderr": subprocess.PIPE})
    result = subprocess.run(cmd, cwd=str(cwd) if cwd else None, **kwargs)
    if check and result.returncode != 0:
        stderr = (result.stderr or "").strip()
        stdout = (result.stdout or "").strip()
        detail = stderr or stdout or f"exit status {result.returncode}"
        raise ReconstructionError(f"command failed: {' '.join(cmd)}\n{detail}")
    return result


def git(repo: Path, *args: str, check: bool = True, capture: bool = True) -> subprocess.CompletedProcess[str]:
    return run(["git", "-C", str(repo), *args], check=check, capture=capture)


def repo_root(start: Path | str | None = None) -> Path:
    base = Path(start) if start else Path(__file__).resolve().parent
    if base.is_file():
        base = base.parent
    result = run(["git", "-C", str(base), "rev-parse", "--show-toplevel"])
    return Path(result.stdout.strip())


def load_json(repo: Path, relative: str) -> dict[str, Any]:
    path = repo / relative
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def truthy(value: str | int | bool | None) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return False
    return str(value).strip().lower() in {"1", "true", "yes", "on"}


def release_to_branch(release: str) -> str:
    if release.startswith("refs/heads/"):
        release = release[len("refs/heads/") :]
    if release.startswith("source/release/"):
        return release
    return f"source/release/{release}"


def branch_to_ref(branch: str) -> str:
    return branch if branch.startswith("refs/") else f"refs/heads/{branch}"


def short_release(branch: str) -> str:
    return branch[len("source/release/") :] if branch.startswith("source/release/") else branch


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_") or "release"


def ref_exists(repo: Path, ref: str) -> bool:
    return git(repo, "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}", check=False).returncode == 0


def rev_parse(repo: Path, ref: str) -> str:
    return git(repo, "rev-parse", f"{ref}^{{commit}}").stdout.strip()


def tree_id(repo: Path, ref: str) -> str:
    return git(repo, "rev-parse", f"{ref}^{{tree}}").stdout.strip()


def fetch_branch_from_origin(repo: Path, branch: str, verbose: bool = False) -> bool:
    refspec = f"refs/heads/{branch}:refs/remotes/origin/{branch}"
    result = git(repo, "fetch", "origin", refspec, check=False, capture=True)
    if result.returncode == 0:
        if verbose:
            print(f"Fetched {branch} from origin", file=sys.stderr)
        return True
    if verbose:
        warning = (result.stderr or result.stdout or "unknown fetch failure").strip()
        print(f"warning: could not fetch {branch} from origin: {warning}", file=sys.stderr)
    return False


def resolve_branch_or_origin(repo: Path, branch: str, verbose: bool = False) -> str | None:
    candidates = [branch, f"origin/{branch}"]
    for candidate in candidates:
        if ref_exists(repo, candidate):
            return candidate
    fetch_branch_from_origin(repo, branch, verbose=verbose)
    for candidate in candidates:
        if ref_exists(repo, candidate):
            return candidate
    return None


def release_entry(repo: Path, release: str | None, require: bool = False) -> tuple[str, dict[str, Any]]:
    releases = load_json(repo, "config/releases.json")
    selected = release or releases.get("default_release")
    if not selected:
        if require:
            raise ReconstructionError("RELEASE is required and no default release is configured")
        raise ReconstructionError("no release selected")
    branch = release_to_branch(selected)
    entries: dict[str, Any] = releases.get("releases", {})
    entry = entries.get(branch) or entries.get(short_release(branch))
    if entry is None:
        raise ReconstructionError(f"unknown release: {selected}\nUse make help-vars to list configured releases.")
    return branch, entry


def cache_dir(repo: Path, *parts: str) -> Path:
    path = repo / ".cache" / "edk2-cix"
    for part in parts:
        path /= part
    path.mkdir(parents=True, exist_ok=True)
    return path


def worktree_paths(repo: Path) -> dict[str, dict[str, str]]:
    result = git(repo, "worktree", "list", "--porcelain")
    entries: dict[str, dict[str, str]] = {}
    current: dict[str, str] = {}
    for line in result.stdout.splitlines():
        if not line:
            if "worktree" in current:
                entries[current["worktree"]] = current
            current = {}
            continue
        key, _, value = line.partition(" ")
        current[key] = value
    if "worktree" in current:
        entries[current["worktree"]] = current
    return entries


def checked_out_worktree(repo: Path, branch: str) -> Path | None:
    full = branch_to_ref(branch)
    for path, data in worktree_paths(repo).items():
        if data.get("branch") == full:
            return Path(path)
    return None


def is_dirty_worktree(path: Path) -> bool:
    result = git(path, "status", "--porcelain", check=True)
    return bool(result.stdout.strip())


def load_ref_records(repo: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    refs_dir = repo / "config" / "refs"
    if not refs_dir.exists():
        return records
    for path in sorted(refs_dir.glob("*.json")):
        data = load_json(repo, f"config/refs/{path.name}")
        for record in data.get("refs", []):
            record = dict(record)
            record.setdefault("manifest", str(path.relative_to(repo)))
            records.append(record)
    return records


def immutable_records(repo: Path) -> list[dict[str, Any]]:
    return [r for r in load_ref_records(repo) if r.get("immutable", False)]


def check_immutable_refs(repo: Path, allow_manifest_update: bool = False, refs: Iterable[str] | None = None) -> None:
    wanted = set(refs or [])
    problems: list[str] = []
    for record in immutable_records(repo):
        ref = record.get("ref")
        if not ref or (wanted and ref not in wanted):
            continue
        if not ref_exists(repo, ref):
            if record.get("optional"):
                continue
            problems.append(f"{ref}: missing locally")
            continue
        expected_oid = record.get("object_id")
        expected_tree = record.get("tree_id")
        actual_oid = rev_parse(repo, ref)
        actual_tree = tree_id(repo, ref)
        if expected_oid and actual_oid != expected_oid and not allow_manifest_update:
            problems.append(f"{ref}: object moved from {expected_oid} to {actual_oid}")
        if expected_tree and actual_tree != expected_tree and not allow_manifest_update:
            problems.append(f"{ref}: tree moved from {expected_tree} to {actual_tree}")
        wt = checked_out_worktree(repo, ref)
        if wt and is_dirty_worktree(wt):
            problems.append(f"{ref}: checked out in dirty worktree {wt}")
    if problems:
        details = "\n".join(f"  - {p}" for p in problems)
        raise ReconstructionError(f"immutable ref check failed:\n{details}")


def die(message: str, code: int = 2) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(code)


def main_wrapper(fn) -> None:
    try:
        fn()
    except ReconstructionError as exc:
        die(str(exc), 2)
