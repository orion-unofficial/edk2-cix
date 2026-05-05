#!/usr/bin/env python3
"""Import developer changes into unofficial source refs explicitly."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
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
    local_compatibility_tag_for_branch,
    main_wrapper,
    ref_exists,
    repo_root,
    rev_parse,
    safe_name,
    truthy,
    version_key,
)


ZERO_OID = "0" * 40
OP_NAMESPACE = "import-unofficial"
CURRENT_REF = "source/unofficial/current"

HELP = """import-unofficial-commits

Required variables:
  FROM_REF=<ref>        Developer topic branch or commit to import from.

Optional variables:
  SOURCE_UNOFFICIAL_REF=source/unofficial/current
                        Unofficial source branch to update for direct imports.
  PROPAGATE_CHECKPOINTS=none|all
                        Replay FROM_REF changes onto every source/unofficial/edk2-stable*
                        checkpoint. The default is none.
  UPDATE_COMPAT_TAGS=0|1
                        With checkpoint propagation, update matching
                        source/unofficial/edk2/stable-* tags after all replays succeed.
  BASE_REF=<ref>        Replay base. Usually inferred from source/unofficial/current.
  ALLOW_SOURCE_REF_FROM=0|1
                        Allow FROM_REF to be a source/unofficial/** ref. This is a
                        maintainer escape hatch and normally should not be needed.
  CONTINUE=0|1          Continue a paused import after conflicts are resolved.
  ABORT=0|1             Remove paused import state without moving refs.
  OP_ID=<id>            Paused operation ID for CONTINUE=1 or ABORT=1.
  WRITE=0|1             Required before refs or tags are created or advanced.
  V=0|1                 Print delegated git operations.

The propagation workflow prepares candidate commits in .cache/edk2-cix first.
Permanent source/unofficial/** branches and compatibility tags are updated only
after every requested replay is clean and guarded old object IDs still match.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--base-ref", default=os.environ.get("BASE_REF", ""))
    p.add_argument("--source-unofficial-ref", default=os.environ.get("SOURCE_UNOFFICIAL_REF", CURRENT_REF))
    p.add_argument("--propagate-checkpoints", default=os.environ.get("PROPAGATE_CHECKPOINTS", "none"))
    p.add_argument("--update-compat-tags", default=os.environ.get("UPDATE_COMPAT_TAGS", os.environ.get("UPDATE_COMPAT_TAG", "0")))
    p.add_argument("--allow-source-ref-from", default=os.environ.get("ALLOW_SOURCE_REF_FROM", "0"))
    p.add_argument("--continue-import", default=os.environ.get("CONTINUE", "0"))
    p.add_argument("--abort", default=os.environ.get("ABORT", "0"))
    p.add_argument("--op-id", default=os.environ.get("OP_ID", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def short_source_ref(ref: str) -> str:
    if ref.startswith("refs/heads/"):
        return ref[len("refs/heads/") :]
    return ref


def source_unofficial_ref(ref: str) -> bool:
    return short_source_ref(ref).startswith("source/unofficial/")


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


def ensure_target_not_checked_out_dirty(repo: Path, ref: str) -> None:
    worktree = checked_out_worktree(repo, ref)
    if not worktree:
        return
    if is_dirty_worktree(worktree):
        raise ReconstructionError(f"{ref} is checked out in dirty worktree {worktree}")
    raise ReconstructionError(f"{ref} is checked out in worktree {worktree}; switch that worktree away before importing")


def is_ancestor(repo: Path, ancestor: str, descendant: str) -> bool:
    result = git(repo, "merge-base", "--is-ancestor", ancestor, descendant, check=False)
    return result.returncode == 0


def merge_base(repo: Path, *args: str) -> str | None:
    result = git(repo, "merge-base", *args, check=False)
    if result.returncode != 0:
        return None
    value = result.stdout.strip()
    return value or None


def infer_base(repo: Path, from_ref: str, explicit_base: str, old_current: str) -> str:
    from_oid = rev_parse(repo, from_ref)
    if explicit_base:
        base_oid = rev_parse(repo, explicit_base)
        if not is_ancestor(repo, base_oid, from_oid):
            raise ReconstructionError(f"BASE_REF is not an ancestor of FROM_REF: {explicit_base}")
        return base_oid

    candidates: list[tuple[str, str]] = []
    if is_ancestor(repo, old_current, from_oid):
        candidates.append((old_current, "source/unofficial/current before import"))

    fork_point = git(repo, "merge-base", "--fork-point", CURRENT_REF, from_ref, check=False)
    if fork_point.returncode == 0 and fork_point.stdout.strip():
        candidates.append((fork_point.stdout.strip(), "fork-point(source/unofficial/current, FROM_REF)"))

    plain_base = merge_base(repo, CURRENT_REF, from_ref)
    if plain_base:
        candidates.append((plain_base, "merge-base(source/unofficial/current, FROM_REF)"))

    unique: dict[str, str] = {}
    for oid, label in candidates:
        if is_ancestor(repo, oid, from_oid):
            unique.setdefault(oid, label)

    if len(unique) == 1:
        return next(iter(unique))
    if not unique:
        raise ReconstructionError(
            "could not infer BASE_REF for replay; re-run with BASE_REF=<old source/unofficial/current commit>"
        )

    lines = ["could not infer BASE_REF unambiguously; candidates:"]
    for index, (oid, label) in enumerate(unique.items(), 1):
        subject = git(repo, "show", "-s", "--format=%s", oid).stdout.strip()
        lines.append(f"  {index}. {oid}  {label}  {subject}")
    lines.append("re-run with BASE_REF=<commit>")
    raise ReconstructionError("\n".join(lines))


def replay_commits(repo: Path, base_oid: str, from_ref: str) -> list[str]:
    result = git(repo, "rev-list", "--reverse", f"{base_oid}..{from_ref}")
    commits = [line for line in result.stdout.splitlines() if line]
    if not commits:
        raise ReconstructionError("replay range is empty; FROM_REF contains no commits after BASE_REF")
    return commits


def checkpoint_targets(repo: Path) -> list[str]:
    refs = local_compatibility_refs(repo)
    if not refs:
        raise ReconstructionError("no source/unofficial/edk2-stable* checkpoints are available")
    return sorted(refs, key=version_key)


def operations_root(repo: Path) -> Path:
    return cache_dir(repo, "operations", OP_NAMESPACE)


def operation_path(repo: Path, op_id: str) -> Path:
    return operations_root(repo) / op_id


def state_path(op_dir: Path) -> Path:
    return op_dir / "state.json"


def load_state(op_dir: Path) -> dict[str, Any]:
    with state_path(op_dir).open("r", encoding="utf-8") as f:
        return json.load(f)


def save_state(op_dir: Path, state: dict[str, Any]) -> None:
    with state_path(op_dir).open("w", encoding="utf-8") as f:
        json.dump(state, f, indent=2, sort_keys=True)
        f.write("\n")


def resolve_operation(repo: Path, op_id: str) -> Path:
    root = operations_root(repo)
    if op_id:
        op_dir = root / op_id
        if not op_dir.exists():
            raise ReconstructionError(f"import operation does not exist: {op_id}")
        return op_dir
    ops = [path for path in root.iterdir() if path.is_dir()] if root.exists() else []
    if len(ops) == 1:
        return ops[0]
    if not ops:
        raise ReconstructionError("no paused import-unofficial operation exists")
    lines = ["multiple paused import-unofficial operations exist; re-run with OP_ID=<id>:"]
    lines.extend(f"  - {path.name}" for path in sorted(ops))
    raise ReconstructionError("\n".join(lines))


def make_op_id(from_ref: str) -> str:
    return f"{int(time.time())}-{safe_name(from_ref)}"


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


def git_config(repo: Path, key: str) -> str:
    result = git(repo, "config", "--get", key, check=False)
    return result.stdout.strip() if result.returncode == 0 else ""


def git_path(repo: Path, name: str) -> Path:
    path = git(repo, "rev-parse", "--git-path", name).stdout.strip()
    result = Path(path)
    return result if result.is_absolute() else repo / result


def cherry_pick_head(repo: Path) -> bool:
    return git_path(repo, "CHERRY_PICK_HEAD").exists()


def unmerged_paths(repo: Path) -> list[str]:
    result = git(repo, "diff", "--name-only", "--diff-filter=U", check=False)
    return [line for line in result.stdout.splitlines() if line]


def cherry_pick_one(repo: Path, commit: str, verbose: bool) -> str:
    result = git(repo, "cherry-pick", "--allow-empty", commit, check=False, capture=not verbose)
    if result.returncode == 0:
        return "applied"
    if unmerged_paths(repo):
        return "conflict"
    if cherry_pick_head(repo):
        git(repo, "cherry-pick", "--skip", capture=not verbose)
        return "skipped"
    detail = (result.stderr or result.stdout).strip()
    raise ReconstructionError(f"cherry-pick failed for {commit}: {detail}")


def apply_remaining(target: dict[str, Any], commits: list[str], verbose: bool) -> bool:
    scratch = Path(target["scratch"])
    index = int(target.get("next_index", 0))
    while index < len(commits):
        status = cherry_pick_one(scratch, commits[index], verbose)
        if status == "conflict":
            target["status"] = "conflict"
            target["next_index"] = index
            return False
        index += 1
        target["next_index"] = index
    target["status"] = "ready"
    target["candidate_oid"] = rev_parse(scratch, "HEAD")
    return True


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


def finalise(repo: Path, op_dir: Path, state: dict[str, Any], verbose: bool) -> None:
    for target in state["targets"]:
        if target.get("status") != "ready":
            raise ReconstructionError(f"cannot finalise; target is not ready: {target['ref']}")
    fetch_candidate_objects(repo, state, verbose)
    updates: list[tuple[str, str, str]] = []
    current = state.get("current_update")
    if current:
        updates.append((branch_to_ref(current["ref"]), current["candidate_oid"], current["old_oid"]))
    for target in state["targets"]:
        updates.append((branch_to_ref(target["ref"]), target["candidate_oid"], target["old_oid"]))
        tag = target.get("tag")
        if tag:
            updates.append((full_tag_ref(tag), target["candidate_oid"], target.get("tag_old_oid") or ZERO_OID))
    transaction_update_refs(repo, updates)
    print("updated unofficial refs:")
    for full_ref, new_oid, _old_oid in updates:
        print(f"  {full_ref} -> {new_oid}")
    shutil.rmtree(op_dir)


def pause_message(op_id: str, target: dict[str, Any]) -> str:
    return (
        "Import paused due to conflicts.\n\n"
        f"Resolve conflicts in:\n  {target['scratch']}\n\n"
        "Then run:\n"
        f"  make import-unofficial-commits CONTINUE=1 OP_ID={op_id} WRITE=1\n\n"
        "Or abort:\n"
        f"  make import-unofficial-commits ABORT=1 OP_ID={op_id}"
    )


def prepare_propagation(repo: Path, args: argparse.Namespace, verbose: bool) -> tuple[Path, dict[str, Any], dict[str, Any] | None]:
    if args.source_unofficial_ref != CURRENT_REF:
        raise ReconstructionError("PROPAGATE_CHECKPOINTS=all updates source/unofficial/current and all checkpoints; do not set SOURCE_UNOFFICIAL_REF")
    if source_unofficial_ref(args.from_ref) and not truthy(args.allow_source_ref_from):
        raise ReconstructionError(
            "FROM_REF must be a topic branch for checkpoint propagation. "
            "Use ALLOW_SOURCE_REF_FROM=1 with explicit BASE_REF only for maintainer recovery workflows."
        )
    old_current = rev_parse(repo, CURRENT_REF)
    from_oid = rev_parse(repo, args.from_ref)
    base_oid = infer_base(repo, args.from_ref, args.base_ref, old_current)
    commits = replay_commits(repo, base_oid, args.from_ref)
    targets = checkpoint_targets(repo)
    for ref in [CURRENT_REF, *targets]:
        ensure_target_not_checked_out_dirty(repo, ref)

    op_id = args.op_id or make_op_id(args.from_ref)
    op_dir = operation_path(repo, op_id)
    if op_dir.exists():
        raise ReconstructionError(f"import operation already exists: {op_id}")
    op_dir.mkdir(parents=True)
    state: dict[str, Any] = {
        "op_id": op_id,
        "from_ref": args.from_ref,
        "from_oid": from_oid,
        "base_ref": args.base_ref,
        "base_oid": base_oid,
        "commits": commits,
        "update_compat_tags": truthy(args.update_compat_tags),
        "current_update": {
            "ref": CURRENT_REF,
            "old_oid": old_current,
            "candidate_oid": from_oid,
        },
        "targets": [],
    }
    for ref in targets:
        target: dict[str, Any] = {
            "ref": ref,
            "old_oid": rev_parse(repo, ref),
            "scratch": str(clone_scratch(repo, op_dir, ref, verbose)),
            "next_index": 0,
            "status": "pending",
        }
        if truthy(args.update_compat_tags):
            tag = local_compatibility_tag_for_branch(ref)
            target["tag"] = tag
            target["tag_old_oid"] = ref_oid(repo, tag, tag=True) or ZERO_OID
        state["targets"].append(target)

    save_state(op_dir, state)
    paused = run_replays(op_dir, state, verbose)
    return op_dir, state, paused


def run_replays(op_dir: Path, state: dict[str, Any], verbose: bool) -> dict[str, Any] | None:
    commits = state["commits"]
    for target in state["targets"]:
        if target.get("status") == "ready":
            continue
        if target.get("status") == "conflict":
            return target
        if not apply_remaining(target, commits, verbose):
            save_state(op_dir, state)
            return target
        save_state(op_dir, state)
    save_state(op_dir, state)
    return None


def continue_operation(repo: Path, op_dir: Path, verbose: bool, write: bool) -> None:
    if not write:
        raise ReconstructionError("WRITE=1 is required to continue and finalise an import")
    state = load_state(op_dir)
    conflicted = [target for target in state["targets"] if target.get("status") == "conflict"]
    if conflicted:
        target = conflicted[0]
        scratch = Path(target["scratch"])
        unresolved = unmerged_paths(scratch)
        if unresolved:
            raise ReconstructionError(
                "import still has unresolved conflicts:\n" + "\n".join(f"  - {path}" for path in unresolved)
            )
        if cherry_pick_head(scratch):
            result = git(scratch, "cherry-pick", "--continue", check=False, capture=not verbose)
            if result.returncode != 0:
                detail = (result.stderr or result.stdout).strip()
                raise ReconstructionError(f"could not continue cherry-pick in {scratch}: {detail}")
        target["next_index"] = int(target.get("next_index", 0)) + 1
        target["status"] = "pending"
        save_state(op_dir, state)

    paused = run_replays(op_dir, state, verbose)
    if paused:
        raise ReconstructionError(pause_message(state["op_id"], paused))
    finalise(repo, op_dir, state, verbose)


def abort_operation(op_dir: Path) -> None:
    shutil.rmtree(op_dir)
    print(f"aborted import operation {op_dir.name}")


def direct_import(repo: Path, args: argparse.Namespace, verbose: bool) -> None:
    ref = args.source_unofficial_ref
    require_unofficial_target(ref)
    if source_unofficial_ref(args.from_ref) and not truthy(args.allow_source_ref_from):
        raise ReconstructionError("FROM_REF must be a topic branch; set ALLOW_SOURCE_REF_FROM=1 only for maintainer recovery workflows")
    if not truthy(args.write):
        print("dry run; set WRITE=1 to update unofficial refs")
        print(f"  {args.from_ref} -> {ref}")
        return
    ensure_target_not_checked_out_dirty(repo, ref)
    from_oid = rev_parse(repo, args.from_ref)
    old_oid = ref_oid(repo, ref) or ZERO_OID
    updates = [(branch_to_ref(ref), from_oid, old_oid)]
    if truthy(args.update_compat_tags):
        tag = local_compatibility_tag_for_branch(ref)
        updates.append((full_tag_ref(tag), from_oid, ref_oid(repo, tag, tag=True) or ZERO_OID))
    transaction_update_refs(repo, updates)
    print(f"updated {ref}")


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)

    if truthy(args.abort):
        abort_operation(resolve_operation(repo, args.op_id))
        return
    if truthy(args.continue_import):
        continue_operation(repo, resolve_operation(repo, args.op_id), verbose, truthy(args.write))
        return

    if not args.from_ref:
        print(HELP)
        print("missing required variable(s): FROM_REF", file=sys.stderr)
        raise SystemExit(2)
    if not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")

    propagate = args.propagate_checkpoints.strip().lower() or "none"
    if propagate not in {"none", "0", "false", "all"}:
        raise ReconstructionError("PROPAGATE_CHECKPOINTS must be none or all")

    if propagate == "all":
        if source_unofficial_ref(args.from_ref) and not truthy(args.allow_source_ref_from):
            raise ReconstructionError(
                "FROM_REF must be a topic branch for checkpoint propagation. "
                "Use ALLOW_SOURCE_REF_FROM=1 with explicit BASE_REF only for maintainer recovery workflows."
            )
        old_current = rev_parse(repo, CURRENT_REF)
        base_oid = infer_base(repo, args.from_ref, args.base_ref, old_current)
        commits = replay_commits(repo, base_oid, args.from_ref)
        targets = checkpoint_targets(repo)
        if not truthy(args.write):
            print("dry run; set WRITE=1 to update unofficial refs")
            print(f"  base: {base_oid}")
            print(f"  commits: {len(commits)}")
            print(f"  update: {CURRENT_REF}")
            for target in targets:
                line = f"  replay: {target}"
                if truthy(args.update_compat_tags):
                    line += f" and {local_compatibility_tag_for_branch(target)}"
                print(line)
            return
        op_dir, state, paused = prepare_propagation(repo, args, verbose)
        if paused:
            raise ReconstructionError(pause_message(state["op_id"], paused))
        finalise(repo, op_dir, state, verbose)
        return

    direct_import(repo, args, verbose)


if __name__ == "__main__":
    main_wrapper(main)
