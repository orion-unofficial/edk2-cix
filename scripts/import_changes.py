#!/usr/bin/env python3
"""Extract changes from a broader source tree into unofficial source refs."""

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

from import_unofficial_commits import (
    CURRENT_REF,
    ZERO_OID,
    checkpoint_targets,
    ensure_target_not_checked_out_dirty,
    full_tag_ref,
    git_config,
    is_ancestor,
    ref_oid,
    transaction_update_refs,
    unmerged_paths,
)
from reconstruction_common import (
    ReconstructionError,
    branch_to_ref,
    cache_dir,
    for_each_ref,
    git,
    local_compatibility_tag_for_branch,
    main_wrapper,
    ref_exists,
    repo_root,
    rev_parse,
    safe_name,
    truthy,
)


OP_NAMESPACE = "import-changes"

HELP = """import-changes

Required variables:
  FROM_REF=<ref>        Branch or commit containing the edited source tree.

Optional variables:
  BASE_REF=<ref>        Source tree before the intended change. If omitted, the
                        importer can infer a unique source/cache/** ancestor,
                        source/unofficial/current ancestor, or retained branch
                        fork point.
  SOURCE_UNOFFICIAL_REF=source/unofficial/current
                        Unofficial source branch to update. Checkpoint
                        propagation requires the default current branch.
  PROPAGATE_CHECKPOINTS=none|all
                        Apply the extracted change to every
                        source/unofficial/edk2-stable* checkpoint. The default
                        is none.
  UPDATE_COMPAT_TAGS=0|1
                        With checkpoint propagation, update matching
                        source/unofficial/edk2/stable-* tags after every
                        checkpoint import succeeds.
  COMMIT_MESSAGE=<text> Commit message for the imported patch.
  CONTINUE=0|1          Continue a paused import after conflicts are resolved.
  ABORT=0|1             Remove paused import state without moving refs.
  OP_ID=<id>            Paused operation ID for CONTINUE=1 or ABORT=1.
  WRITE=0|1             Required before refs or tags are created or advanced.
  V=0|1                 Print delegated git operations.

This target is for changes developed on materialised source/cache/** branches,
legacy source branches, or other broader trees. Use import-unofficial-commits
instead when FROM_REF is already a topic branch based on source/unofficial/current.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--base-ref", default=os.environ.get("BASE_REF", ""))
    p.add_argument("--source-unofficial-ref", default=os.environ.get("SOURCE_UNOFFICIAL_REF", CURRENT_REF))
    p.add_argument("--propagate-checkpoints", default=os.environ.get("PROPAGATE_CHECKPOINTS", "none"))
    p.add_argument("--update-compat-tags", default=os.environ.get("UPDATE_COMPAT_TAGS", "0"))
    p.add_argument("--commit-message", default=os.environ.get("COMMIT_MESSAGE", ""))
    p.add_argument("--continue-import", default=os.environ.get("CONTINUE", "0"))
    p.add_argument("--abort", default=os.environ.get("ABORT", "0"))
    p.add_argument("--op-id", default=os.environ.get("OP_ID", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


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
        raise ReconstructionError("no paused import-changes operation exists")
    lines = ["multiple paused import-changes operations exist; re-run with OP_ID=<id>:"]
    lines.extend(f"  - {path.name}" for path in sorted(ops))
    raise ReconstructionError("\n".join(lines))


def make_op_id(from_ref: str) -> str:
    return f"{int(time.time())}-{safe_name(from_ref)}"


def require_unofficial_target(ref: str) -> None:
    if not ref.startswith("source/unofficial/"):
        raise ReconstructionError("SOURCE_UNOFFICIAL_REF must be under source/unofficial/")


def nearest_ancestor(repo: Path, from_ref: str, refs: list[str]) -> tuple[str, str] | None:
    from_oid = rev_parse(repo, from_ref)
    candidates: list[tuple[int, str, str]] = []
    for ref in refs:
        oid = rev_parse(repo, ref)
        if oid == from_oid:
            continue
        if is_ancestor(repo, oid, from_oid):
            count = int(git(repo, "rev-list", "--count", f"{oid}..{from_ref}").stdout.strip())
            candidates.append((count, ref, oid))
    if not candidates:
        return None
    candidates.sort(key=lambda item: (item[0], item[1]))
    if len(candidates) > 1 and candidates[0][0] == candidates[1][0]:
        tied = "\n".join(f"  - {ref}" for _count, ref, _oid in candidates if _count == candidates[0][0])
        raise ReconstructionError(f"could not infer BASE_REF unambiguously; candidates:\n{tied}\nre-run with BASE_REF=<ref>")
    _count, ref, oid = candidates[0]
    return ref, oid


def merge_base(repo: Path, left: str, right: str) -> str | None:
    result = git(repo, "merge-base", left, right, check=False)
    if result.returncode != 0:
        return None
    value = result.stdout.strip()
    return value or None


def branch_heads(repo: Path) -> list[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", "refs/heads", check=False)
    if result.returncode != 0:
        return []
    return [line for line in result.stdout.splitlines() if line]


def broader_base_refs(repo: Path, from_ref: str) -> list[str]:
    from_short = from_ref.removeprefix("refs/heads/")
    refs: list[str] = []
    for ref in branch_heads(repo):
        if ref == from_short:
            continue
        if ref == "build" or ref.startswith("source/cache/") or ref.startswith("source/unofficial/"):
            continue
        refs.append(ref)
    return refs


def base_label_sort_key(label: str) -> tuple[int, str]:
    ref = label
    if label.startswith("merge-base("):
        ref = label[len("merge-base(") :].split(",", 1)[0]
    if ref == "main-monorepo":
        return (0, label)
    if ref == "main" or ref.startswith("main-monorepo"):
        return (1, label)
    if ref.startswith("codex/"):
        return (9, label)
    return (5, label)


def infer_broader_base(repo: Path, from_ref: str, refs: list[str]) -> tuple[str, str] | None:
    from_oid = rev_parse(repo, from_ref)
    candidates_by_oid: dict[str, dict[str, Any]] = {}
    for ref in refs:
        oid = rev_parse(repo, ref)
        if oid == from_oid:
            continue
        base_oid = merge_base(repo, ref, from_ref)
        if not base_oid or base_oid == from_oid:
            continue
        count = int(git(repo, "rev-list", "--count", f"{base_oid}..{from_ref}").stdout.strip())
        label = ref if oid == base_oid else f"merge-base({ref}, FROM_REF)"
        record = candidates_by_oid.setdefault(base_oid, {"count": count, "labels": []})
        record["count"] = min(int(record["count"]), count)
        record["labels"].append(label)

    if not candidates_by_oid:
        return None

    ordered = sorted(
        (
            (int(record["count"]), oid, sorted(set(record["labels"]), key=base_label_sort_key))
            for oid, record in candidates_by_oid.items()
        ),
        key=lambda item: (item[0], item[1]),
    )
    best_count = ordered[0][0]
    best = [item for item in ordered if item[0] == best_count]
    if len(best) > 1:
        lines = ["could not infer BASE_REF unambiguously; candidate fork points:"]
        for _count, oid, labels in best:
            lines.append(f"  - {oid} from {', '.join(labels)}")
        lines.append("re-run with BASE_REF=<ref>")
        raise ReconstructionError("\n".join(lines))
    _count, oid, labels = best[0]
    return labels[0], oid


def infer_base(repo: Path, from_ref: str, explicit_base: str, target_ref: str) -> tuple[str, str]:
    from_oid = rev_parse(repo, from_ref)
    if explicit_base:
        base_oid = rev_parse(repo, explicit_base)
        if not is_ancestor(repo, base_oid, from_oid):
            raise ReconstructionError(f"BASE_REF is not an ancestor of FROM_REF: {explicit_base}")
        return explicit_base, base_oid

    cache_base = nearest_ancestor(repo, from_ref, for_each_ref(repo, "source/cache"))
    if cache_base:
        return cache_base

    target_base = nearest_ancestor(repo, from_ref, [target_ref])
    if target_base:
        return target_base

    broader_base = infer_broader_base(repo, from_ref, broader_base_refs(repo, from_ref))
    if broader_base:
        return broader_base

    raise ReconstructionError(
        "could not infer BASE_REF. Re-run with BASE_REF=<source tree before the intended change>."
    )


def changed_files(repo: Path, base_oid: str, from_ref: str) -> list[str]:
    result = git(repo, "diff", "--name-status", f"{base_oid}..{from_ref}")
    return [line for line in result.stdout.splitlines() if line]


def write_patch(repo: Path, base_oid: str, from_ref: str, patch_path: Path) -> list[str]:
    changes = changed_files(repo, base_oid, from_ref)
    if not changes:
        raise ReconstructionError("change diff is empty; FROM_REF contains no tree changes after BASE_REF")
    patch_path.parent.mkdir(parents=True, exist_ok=True)
    patch = git(repo, "diff", "--binary", "--full-index", f"{base_oid}..{from_ref}")
    patch_path.write_text(patch.stdout, encoding="utf-8")
    return changes


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


def staged_changes(repo: Path) -> bool:
    return git(repo, "diff", "--cached", "--quiet", check=False).returncode != 0


def dirty_paths(repo: Path) -> list[str]:
    result = git(repo, "status", "--porcelain", check=False)
    return [line for line in result.stdout.splitlines() if line]


def commit_scratch(repo: Path, message: str) -> str:
    unresolved = unmerged_paths(repo)
    if unresolved:
        raise ReconstructionError("import still has unresolved conflicts:\n" + "\n".join(f"  - {path}" for path in unresolved))
    if not staged_changes(repo):
        dirty = dirty_paths(repo)
        if dirty:
            raise ReconstructionError(
                "resolved files are not staged in the scratch tree. Run git add for the resolved files, then continue the import."
            )
        raise ReconstructionError("import patch produced no staged changes")
    git(repo, "commit", "-m", message)
    return rev_parse(repo, "HEAD")


def apply_patch_to_target(target: dict[str, Any], patch_path: Path, message: str, verbose: bool) -> bool:
    scratch = Path(target["scratch"])
    result = subprocess.run(
        ["git", "-C", str(scratch), "apply", "--3way", "--index", "--binary", str(patch_path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode == 0:
        target["status"] = "ready"
        target["candidate_oid"] = commit_scratch(scratch, message)
        return True
    if unmerged_paths(scratch):
        target["status"] = "conflict"
        target["apply_stdout"] = result.stdout
        target["apply_stderr"] = result.stderr
        return False
    target["status"] = "conflict"
    target["apply_stdout"] = result.stdout
    target["apply_stderr"] = result.stderr
    return False


def build_targets(repo: Path, args: argparse.Namespace) -> list[dict[str, Any]]:
    require_unofficial_target(args.source_unofficial_ref)
    propagate = args.propagate_checkpoints.strip().lower() or "none"
    if propagate not in {"none", "0", "false", "all"}:
        raise ReconstructionError("PROPAGATE_CHECKPOINTS must be none or all")
    if propagate == "all" and args.source_unofficial_ref != CURRENT_REF:
        raise ReconstructionError("PROPAGATE_CHECKPOINTS=all updates source/unofficial/current and all checkpoints; do not set SOURCE_UNOFFICIAL_REF")

    refs = [args.source_unofficial_ref]
    if propagate == "all":
        refs.extend(checkpoint_targets(repo))

    targets: list[dict[str, Any]] = []
    for ref in refs:
        target: dict[str, Any] = {
            "ref": ref,
            "old_oid": rev_parse(repo, ref),
            "status": "pending",
        }
        if propagate == "all" and ref != args.source_unofficial_ref and truthy(args.update_compat_tags):
            tag = local_compatibility_tag_for_branch(ref)
            target["tag"] = tag
            target["tag_old_oid"] = ref_oid(repo, tag, tag=True) or ZERO_OID
        targets.append(target)
    return targets


def pause_message(op_id: str, targets: list[dict[str, Any]]) -> str:
    lines = ["Import paused due to conflicts.", "", "Resolve conflicts in:"]
    for target in targets:
        if target.get("status") == "conflict":
            lines.append(f"  {target['scratch']}")
            detail = (target.get("apply_stderr") or target.get("apply_stdout") or "").strip()
            if detail:
                lines.append("  apply output:")
                lines.extend(f"    {line}" for line in detail.splitlines()[:12])
    lines.extend(
        [
            "",
            "If Git did not create conflict markers, apply the intended change",
            "manually in the scratch tree, stage the resolved files with git add,",
            "and then continue.",
            "",
            "Then run:",
            f"  make import-changes CONTINUE=1 OP_ID={op_id} WRITE=1",
            "",
            "Or abort:",
            f"  make import-changes ABORT=1 OP_ID={op_id}",
        ]
    )
    return "\n".join(lines)


def prepare_operation(repo: Path, args: argparse.Namespace, verbose: bool) -> tuple[Path, dict[str, Any], list[dict[str, Any]]]:
    base_ref, base_oid = infer_base(repo, args.from_ref, args.base_ref, args.source_unofficial_ref)
    from_oid = rev_parse(repo, args.from_ref)
    targets = build_targets(repo, args)
    for target in targets:
        ensure_target_not_checked_out_dirty(repo, target["ref"])

    op_id = args.op_id or make_op_id(args.from_ref)
    op_dir = operation_path(repo, op_id)
    if op_dir.exists():
        raise ReconstructionError(f"import operation already exists: {op_id}")
    op_dir.mkdir(parents=True)
    patch_path = op_dir / "change.patch"
    changes = write_patch(repo, base_oid, args.from_ref, patch_path)
    message = args.commit_message or f"import: changes from {args.from_ref}"
    state: dict[str, Any] = {
        "op_id": op_id,
        "from_ref": args.from_ref,
        "from_oid": from_oid,
        "base_ref": base_ref,
        "base_oid": base_oid,
        "patch_path": str(patch_path),
        "changes": changes,
        "message": message,
        "targets": targets,
    }

    paused: list[dict[str, Any]] = []
    for target in targets:
        target["scratch"] = str(clone_scratch(repo, op_dir, target["ref"], verbose))
        if not apply_patch_to_target(target, patch_path, message, verbose):
            paused.append(target)
        save_state(op_dir, state)
    return op_dir, state, paused


def fetch_candidate_objects(repo: Path, state: dict[str, Any], verbose: bool) -> None:
    for target in state["targets"]:
        if target.get("candidate_oid"):
            git(repo, "fetch", "--no-tags", str(Path(target["scratch"])), target["candidate_oid"], capture=not verbose)


def finalise(repo: Path, op_dir: Path, state: dict[str, Any], verbose: bool) -> None:
    not_ready = [target["ref"] for target in state["targets"] if target.get("status") != "ready"]
    if not_ready:
        raise ReconstructionError("cannot finalise; target is not ready:\n" + "\n".join(f"  - {ref}" for ref in not_ready))
    fetch_candidate_objects(repo, state, verbose)
    updates: list[tuple[str, str, str]] = []
    for target in state["targets"]:
        updates.append((branch_to_ref(target["ref"]), target["candidate_oid"], target["old_oid"]))
        if target.get("tag"):
            updates.append((full_tag_ref(target["tag"]), target["candidate_oid"], target.get("tag_old_oid") or ZERO_OID))
    transaction_update_refs(repo, updates)
    print("updated unofficial refs:")
    for full_ref, new_oid, _old_oid in updates:
        print(f"  {full_ref} -> {new_oid}")
    shutil.rmtree(op_dir)


def continue_operation(repo: Path, op_dir: Path, verbose: bool, write: bool) -> None:
    if not write:
        raise ReconstructionError("WRITE=1 is required to continue and finalise an import")
    state = load_state(op_dir)
    paused: list[dict[str, Any]] = []
    for target in state["targets"]:
        if target.get("status") != "conflict":
            continue
        scratch = Path(target["scratch"])
        try:
            target["candidate_oid"] = commit_scratch(scratch, state["message"])
            target["status"] = "ready"
        except ReconstructionError:
            paused.append(target)
    save_state(op_dir, state)
    if paused:
        raise ReconstructionError(pause_message(state["op_id"], paused))
    finalise(repo, op_dir, state, verbose)


def abort_operation(op_dir: Path) -> None:
    shutil.rmtree(op_dir)
    print(f"aborted import operation {op_dir.name}")


def print_dry_run(state: dict[str, Any]) -> None:
    print("dry run; set WRITE=1 to update unofficial refs")
    print(f"  base: {state['base_ref']} ({state['base_oid']})")
    print(f"  from: {state['from_ref']} ({state['from_oid']})")
    print("  changed paths:")
    for change in state["changes"]:
        print(f"    {change}")
    print("  update targets:")
    for target in state["targets"]:
        line = f"    {target['ref']}"
        if target.get("tag"):
            line += f" and {target['tag']}"
        print(line)


def dry_run_conflict_message(paused: list[dict[str, Any]]) -> str:
    lines = [
        "dry run detected conflicts while applying the extracted patch.",
        "No refs were moved and the temporary scratch trees were removed.",
        "Re-run with WRITE=1 to keep scratch state for conflict resolution.",
        "",
        "Conflicting target(s):",
    ]
    lines.extend(f"  - {target['ref']}" for target in paused)
    lines.extend(
        [
            "",
            "Some patches may require manual application when re-run with WRITE=1.",
        ]
    )
    return "\n".join(lines)


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

    if not truthy(args.write):
        op_dir, state, paused = prepare_operation(repo, args, verbose)
        try:
            print_dry_run(state)
            if paused:
                raise ReconstructionError(dry_run_conflict_message(paused))
        finally:
            shutil.rmtree(op_dir)
        return

    op_dir, state, paused = prepare_operation(repo, args, verbose)
    if paused:
        raise ReconstructionError(pause_message(state["op_id"], paused))
    finalise(repo, op_dir, state, verbose)


if __name__ == "__main__":
    main_wrapper(main)
