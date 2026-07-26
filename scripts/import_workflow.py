#!/usr/bin/env python3
"""Shared helpers for safe unofficial-source import workflows."""

from __future__ import annotations

import json
import os
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
    clear_metadata_caches,
    git,
    is_dirty_worktree,
    selected_unofficial_current_ref,
    unofficial_release_branches,
    rev_parse,
    safe_name,
    version_key,
)


ZERO_OID = "0" * 40


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


def current_unofficial_ref(repo: Path, requested: str = "") -> str:
    ref = requested.strip() or selected_unofficial_current_ref(repo)
    require_unofficial_target(ref)
    return ref


def is_ancestor(repo: Path, ancestor: str, descendant: str) -> bool:
    result = git(repo, "merge-base", "--is-ancestor", ancestor, descendant, check=False)
    return result.returncode == 0


def merge_base(repo: Path, *refs: str) -> str | None:
    result = git(repo, "merge-base", *refs, check=False)
    if result.returncode != 0:
        return None
    value = result.stdout.strip()
    return value or None


def release_branch_targets(repo: Path) -> list[str]:
    refs = unofficial_release_branches(repo)
    if not refs:
        raise ReconstructionError("no source/unofficial/edk2-stable* release branches are available")
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


def operation_short_id(op_id: str) -> str:
    return op_id.split("-", 1)[0]


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
        if op_dir.exists():
            return op_dir
        if op_id.isdigit() and root.exists():
            matches = sorted(path for path in root.iterdir() if path.is_dir() and path.name.startswith(f"{op_id}-"))
            if len(matches) == 1:
                return matches[0]
            if len(matches) > 1:
                lines = [f"multiple paused {label} operations match OP_ID={op_id}; re-run with the full OP_ID:"]
                lines.extend(f"  - {path.name}" for path in matches)
                raise ReconstructionError("\n".join(lines))
        raise ReconstructionError(f"{label} operation does not exist: {op_id}")
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


def repo_from_operation_dir(op_dir: Path) -> Path | None:
    for parent in op_dir.parents:
        if parent.name == ".cache":
            return parent.parent
    return None


def shortcut_points_inside(link: Path, op_dir: Path) -> bool:
    try:
        target = Path(os.readlink(link))
    except OSError:
        return False
    if not target.is_absolute():
        target = link.parent / target
    try:
        target.resolve().relative_to(op_dir.resolve())
        return True
    except ValueError:
        return False


def create_scratch_shortcuts(repo: Path, op_id: str, targets: list[dict[str, Any]]) -> None:
    short = operation_short_id(op_id)
    if not short.isdigit():
        return
    scratch_targets = [target for target in targets if target.get("scratch")]
    if not scratch_targets:
        return
    primary = scratch_targets[0]
    if len(scratch_targets) == 1:
        primary = scratch_targets[0]

    for target in scratch_targets:
        names: list[str]
        if len(scratch_targets) == 1:
            names = [short]
        else:
            names = [f"{short}-{safe_name(str(target['ref']))}"]
            if target is primary:
                names.insert(0, short)

        scratch = Path(str(target["scratch"]))
        created: list[str] = []
        for name in names:
            link = repo / name
            if link.exists() or link.is_symlink():
                if link.is_symlink() and shortcut_points_inside(link, Path(str(target["scratch"])).parents[1]):
                    link.unlink()
                else:
                    raise ReconstructionError(f"cannot create scratch shortcut {name}: path already exists")
            os.symlink(os.path.relpath(scratch, repo), link)
            created.append(name)
        if created:
            target["scratch_shortcut"] = created[0]


def cleanup_operation_shortcuts(op_dir: Path) -> None:
    repo = repo_from_operation_dir(op_dir)
    if not repo:
        return
    short = operation_short_id(op_dir.name)
    if not short.isdigit():
        return
    for link in repo.glob(f"{short}*"):
        if link.is_symlink() and shortcut_points_inside(link, op_dir):
            link.unlink()


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
    clear_metadata_caches()


def abort_operation(op_dir: Path, label: str) -> None:
    cleanup_operation_shortcuts(op_dir)
    shutil.rmtree(op_dir)
    print(f"aborted {label} operation {op_dir.name}")


def remove_operation_state(op_dir: Path, *, ignore_errors: bool = False) -> None:
    cleanup_operation_shortcuts(op_dir)
    shutil.rmtree(op_dir, ignore_errors=ignore_errors)


def import_receipt_path(repo: Path, target_ref: str) -> Path:
    return cache_dir(repo, "operations", "import-receipts") / f"last-{safe_name(target_ref)}.json"


def write_current_import_receipt(
    repo: Path,
    *,
    tool: str,
    from_ref: str,
    base_ref: str,
    base_oid: str,
    old_oid: str,
    new_oid: str,
    target_ref: str,
) -> None:
    path = import_receipt_path(repo, target_ref)
    payload = {
        "base_oid": base_oid,
        "base_ref": base_ref,
        "from_ref": from_ref,
        "new_oid": new_oid,
        "old_oid": old_oid,
        "target_ref": target_ref,
        "tool": tool,
    }
    with path.open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)
        f.write("\n")


def read_current_import_receipt(repo: Path, target_ref: str) -> dict[str, Any] | None:
    path = import_receipt_path(repo, target_ref)
    if not path.exists():
        return None
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        return None
    return data
