#!/usr/bin/env python3
"""Import developer changes into unofficial source refs explicitly."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Any

from import_workflow import (
    CURRENT_REF,
    ZERO_OID,
    abort_operation,
    checkpoint_targets,
    cherry_pick_head,
    clone_scratch,
    ensure_target_not_checked_out_dirty,
    fetch_candidate_objects,
    full_tag_ref,
    is_ancestor,
    load_state,
    make_op_id,
    merge_base,
    operation_path,
    ref_oid,
    remove_operation_state,
    require_unofficial_target,
    resolve_operation,
    save_state,
    transaction_update_refs,
    unmerged_paths,
)
from reconstruction_common import (
    ReconstructionError,
    branch_to_ref,
    for_each_ref,
    git,
    local_compatibility_tag_for_branch,
    main_wrapper,
    ref_exists,
    repo_root,
    rev_parse,
    truthy,
)
from source_policy import enforce_source_tree_policy


OP_NAMESPACE = "import-unofficial"

HELP = """import-unofficial-commits

Required variables:
  FROM_REF=<ref>        Developer topic branch or commit to import from.
                        The ref must already be based on the target
                        source/unofficial/** branch.

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

Use import-changes instead when the change was developed on a materialised
source/cache/** branch, a legacy source branch, or any broader source tree.

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
    for prefix in ("refs/heads/", "refs/remotes/origin/"):
        if ref.startswith(prefix):
            return ref[len(prefix) :]
    return ref


def source_unofficial_ref(ref: str) -> bool:
    return short_source_ref(ref).startswith("source/unofficial/")


def source_cache_ref(ref: str) -> bool:
    return short_source_ref(ref).startswith("source/cache/")


def cache_ancestor_refs(repo: Path, from_ref: str) -> list[str]:
    from_oid = rev_parse(repo, from_ref)
    ancestors: list[str] = []
    for ref in for_each_ref(repo, "source/cache"):
        oid = rev_parse(repo, ref)
        if oid != from_oid and is_ancestor(repo, oid, from_oid):
            ancestors.append(ref)
    return ancestors


def require_valid_import_source(repo: Path, from_ref: str, target_ref: str, allow_source_ref_from: bool) -> None:
    if source_unofficial_ref(from_ref) and not allow_source_ref_from:
        raise ReconstructionError("FROM_REF must be a topic branch; set ALLOW_SOURCE_REF_FROM=1 only for maintainer recovery workflows")
    if source_cache_ref(from_ref):
        raise ReconstructionError(
            "FROM_REF is a generated source/cache/** ref. Use make import-changes to extract changes from materialised or broader source trees."
        )
    cache_ancestors = cache_ancestor_refs(repo, from_ref)
    if cache_ancestors:
        refs = "\n".join(f"  - {ref}" for ref in cache_ancestors[:10])
        extra = "" if len(cache_ancestors) <= 10 else f"\n  ... and {len(cache_ancestors) - 10} more"
        raise ReconstructionError(
            "FROM_REF appears to be based on generated source/cache/** content, which is not a valid input for import-unofficial-commits.\n"
            "Use make import-changes to extract the intended diff onto source/unofficial/current.\n"
            f"Detected cache ancestor(s):\n{refs}{extra}"
        )
    if not is_ancestor(repo, rev_parse(repo, target_ref), rev_parse(repo, from_ref)):
        raise ReconstructionError(
            f"FROM_REF is not based on {target_ref}. "
            "Use make import-changes with BASE_REF=<base> to extract changes from a broader source tree."
        )


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
    enforce_source_tree_policy(scratch, ref="HEAD", label=f"import scratch for {target['ref']}")
    target["status"] = "ready"
    target["candidate_oid"] = rev_parse(scratch, "HEAD")
    return True


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
    remove_operation_state(op_dir)


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
    require_valid_import_source(repo, args.from_ref, CURRENT_REF, truthy(args.allow_source_ref_from))
    old_current = rev_parse(repo, CURRENT_REF)
    from_oid = rev_parse(repo, args.from_ref)
    enforce_source_tree_policy(repo, ref=args.from_ref, label=args.from_ref)
    base_oid = infer_base(repo, args.from_ref, args.base_ref, old_current)
    commits = replay_commits(repo, base_oid, args.from_ref)
    targets = checkpoint_targets(repo)
    for ref in [CURRENT_REF, *targets]:
        ensure_target_not_checked_out_dirty(repo, ref)

    op_id = args.op_id or make_op_id(args.from_ref)
    op_dir = operation_path(repo, OP_NAMESPACE, op_id)
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


def direct_import(repo: Path, args: argparse.Namespace, verbose: bool) -> None:
    ref = args.source_unofficial_ref
    require_unofficial_target(ref)
    require_valid_import_source(repo, args.from_ref, ref, truthy(args.allow_source_ref_from))
    enforce_source_tree_policy(repo, ref=args.from_ref, label=args.from_ref)
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
        abort_operation(resolve_operation(repo, OP_NAMESPACE, "import-unofficial", args.op_id), "import-unofficial")
        return
    if truthy(args.continue_import):
        continue_operation(repo, resolve_operation(repo, OP_NAMESPACE, "import-unofficial", args.op_id), verbose, truthy(args.write))
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
        require_valid_import_source(repo, args.from_ref, CURRENT_REF, truthy(args.allow_source_ref_from))
        enforce_source_tree_policy(repo, ref=args.from_ref, label=args.from_ref)
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
