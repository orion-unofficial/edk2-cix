#!/usr/bin/env python3
"""Shared helpers for safe unofficial-source import workflows."""

from __future__ import annotations

import json
import shutil
import subprocess
import time
from pathlib import Path
from typing import Any

from reconstruction_common import (
    ReconstructionError,
    branch_to_ref,
    cache_dir,
    checked_out_worktree,
    git,
    is_dirty_worktree,
    local_compatibility_refs,
    rev_parse,
    safe_name,
    version_key,
)


ZERO_OID = "0" * 40
CURRENT_REF = "source/unofficial/current"


def full_tag_ref(tag: str) -> str:
    return tag if tag.startswith("refs/tags/") else f"refs/tags/{tag}"


def ref_oid(repo: Path, ref: str, *, tag: bool = False) -> str | None:
    full = full_tag_ref(ref) if tag else branch_to_ref(ref)
    result = git(repo, "rev-parse", "--verify", "--quiet", f"{full}^{{commit}}", check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def require_unofficial_target(ref: str) -> None:
    if not ref.startswith("source/unofficial/"):
        raise ReconstructionError("SOURCE_UNOFFICIAL_REF must be under source/unofficial/")


def is_ancestor(repo: Path, ancestor: str, descendant: str) -> bool:
    result = git(repo, "merge-base", "--is-ancestor", ancestor, descendant, check=False)
    return result.returncode == 0


def merge_base(repo: Path, *refs: str) -> str | None:
    result = git(repo, "merge-base", *refs, check=False)
    if result.returncode != 0:
        return None
    value = result.stdout.strip()
    return value or None


def checkpoint_targets(repo: Path) -> list[str]:
    refs = local_compatibility_refs(repo)
    if not refs:
        raise ReconstructionError("no source/unofficial/edk2-stable* checkpoints are available")
    return sorted(refs, key=version_key)


def ensure_target_not_checked_out_dirty(repo: Path, ref: str) -> None:
    worktree = checked_out_worktree(repo, ref)
    if not worktree:
        return
    if is_dirty_worktree(worktree):
        raise ReconstructionError(f"{ref} is checked out in dirty worktree {worktree}")
    raise ReconstructionError(f"{ref} is checked out in worktree {worktree}; switch that worktree away before importing")


def operations_root(repo: Path, namespace: str) -> Path:
    return cache_dir(repo, "operations", namespace)


def operation_path(repo: Path, namespace: str, op_id: str) -> Path:
    return operations_root(repo, namespace) / op_id


def state_path(op_dir: Path) -> Path:
    return op_dir / "state.json"


def load_state(op_dir: Path) -> dict[str, Any]:
    with state_path(op_dir).open("r", encoding="utf-8") as f:
        return json.load(f)


def save_state(op_dir: Path, state: dict[str, Any]) -> None:
    with state_path(op_dir).open("w", encoding="utf-8") as f:
        json.dump(state, f, indent=2, sort_keys=True)
        f.write("\n")


def resolve_operation(repo: Path, namespace: str, label: str, op_id: str) -> Path:
    root = operations_root(repo, namespace)
    if op_id:
        op_dir = root / op_id
        if not op_dir.exists():
            raise ReconstructionError(f"{label} operation does not exist: {op_id}")
        return op_dir
    ops = [path for path in root.iterdir() if path.is_dir()] if root.exists() else []
    if len(ops) == 1:
        return ops[0]
    if not ops:
        raise ReconstructionError(f"no paused {label} operation exists")
    lines = [f"multiple paused {label} operations exist; re-run with OP_ID=<id>:"]
    lines.extend(f"  - {path.name}" for path in sorted(ops))
    raise ReconstructionError("\n".join(lines))


def make_op_id(from_ref: str) -> str:
    return f"{int(time.time())}-{safe_name(from_ref)}"


def git_config(repo: Path, key: str) -> str:
    result = git(repo, "config", "--get", key, check=False)
    return result.stdout.strip() if result.returncode == 0 else ""


def clone_scratch(repo: Path, op_dir: Path, ref: str, verbose: bool) -> Path:
    scratch_root = op_dir / "scratch"
    scratch_root.mkdir(parents=True, exist_ok=True)
    scratch = scratch_root / safe_name(ref)
    if scratch.exists():
        shutil.rmtree(scratch)
    git(repo, "clone", "--shared", "--no-checkout", str(repo), str(scratch), capture=not verbose)
    git(scratch, "switch", "--detach", rev_parse(repo, ref), capture=not verbose)
    git(scratch, "config", "user.name", git_config(repo, "user.name") or "edk2-cix importer")
    git(scratch, "config", "user.email", git_config(repo, "user.email") or "edk2-cix-import")
    return scratch


def git_path(repo: Path, name: str) -> Path:
    path = git(repo, "rev-parse", "--git-path", name).stdout.strip()
    result = Path(path)
    return result if result.is_absolute() else repo / result


def cherry_pick_head(repo: Path) -> bool:
    return git_path(repo, "CHERRY_PICK_HEAD").exists()


def unmerged_paths(repo: Path) -> list[str]:
    result = git(repo, "diff", "--name-only", "--diff-filter=U", check=False)
    return [line for line in result.stdout.splitlines() if line]


def fetch_candidate_objects(repo: Path, state: dict[str, Any], verbose: bool) -> None:
    for target in state["targets"]:
        if target.get("candidate_oid"):
            git(repo, "fetch", "--no-tags", str(Path(target["scratch"])), target["candidate_oid"], capture=not verbose)


def current_ref_value(repo: Path, full_ref: str) -> str:
    result = git(repo, "rev-parse", "--verify", "--quiet", f"{full_ref}^{{commit}}", check=False)
    if result.returncode != 0:
        return ZERO_OID
    return result.stdout.strip()


def check_old_values(repo: Path, updates: list[tuple[str, str, str]]) -> None:
    problems = []
    for full_ref, _new_oid, old_oid in updates:
        actual = current_ref_value(repo, full_ref)
        if actual != old_oid:
            problems.append(f"{full_ref} changed during import: expected {old_oid}, found {actual}")
    if problems:
        raise ReconstructionError("ref guard check failed:\n" + "\n".join(f"  - {p}" for p in problems))


def transaction_update_refs(repo: Path, updates: list[tuple[str, str, str]]) -> None:
    if not updates:
        return
    check_old_values(repo, updates)
    lines = ["start"]
    for full_ref, new_oid, old_oid in updates:
        lines.append(f"update {full_ref} {new_oid} {old_oid}")
    lines.extend(["prepare", "commit"])
    proc = subprocess.run(
        ["git", "-C", str(repo), "update-ref", "--stdin"],
        input=("\n".join(lines) + "\n"),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        raise ReconstructionError(proc.stderr.strip() or proc.stdout.strip() or "git update-ref transaction failed")


def abort_operation(op_dir: Path, label: str) -> None:
    shutil.rmtree(op_dir)
    print(f"aborted {label} operation {op_dir.name}")


def remove_operation_state(op_dir: Path, *, ignore_errors: bool = False) -> None:
    shutil.rmtree(op_dir, ignore_errors=ignore_errors)
