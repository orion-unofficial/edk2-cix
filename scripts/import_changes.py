#!/usr/bin/env python3
"""Extract changes from a broader source tree into unofficial source refs."""

from __future__ import annotations

import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Any

from check_identity_integrity import scan_commit_message_for_legacy_branch
from inspect_import_conflicts import write_conflict_report
from import_workflow import (
    CURRENT_REF,
    ZERO_OID,
    abort_operation,
    clone_scratch,
    ensure_target_not_checked_out_dirty,
    fetch_candidate_objects,
    full_tag_ref,
    is_ancestor,
    load_state,
    make_op_id,
    merge_base,
    operations_root,
    operation_path,
    ref_oid,
    remove_operation_state,
    require_unofficial_target,
    resolve_operation,
    release_branch_targets,
    save_state,
    transaction_update_refs,
    unmerged_paths,
    write_current_import_receipt,
)
from reconstruction_common import (
    ReconstructionError,
    branch_to_ref,
    for_each_ref,
    git,
    unofficial_release_tag_for_branch,
    main_wrapper,
    ref_exists,
    repo_root,
    rev_parse,
    safe_name,
    truthy,
)
from source_policy import enforce_source_tree_policy
from source_lifecycle import (
    changed_overlay_paths_from_name_status,
    normalise_mode,
    normalise_overlay_lifecycle,
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
                        Unofficial source branch to update. Release-branch
                        propagation requires the default current branch.
  PROPAGATE_RELEASE_BRANCHES=none|all
                        Apply the extracted change to every
                        source/unofficial/edk2-stable* release branch. The
                        default is none.
  UPDATE_RELEASE_TAGS=0|1
                        Only valid with PROPAGATE_RELEASE_BRANCHES=all. Update
                        matching source/unofficial/edk2/stable-* tags after
                        every release-branch import succeeds. The safer staged
                        workflow is to run make update-release-tags separately
                        after validating the propagated release branches.
  COMMIT_MESSAGE=<text> Commit message for the imported patch. Literal \\n
                        sequences are mapped to separate git commit -m
                        paragraphs. If no message input is set, the FROM_REF
                        tip commit message is inherited.
  COMMIT_MESSAGE_FILE=<path>
                        File containing the commit message for the imported
                        patch.
  SIGNOFF=0|1           Add a Signed-off-by trailer with git commit -s.
  SOURCE_LIFECYCLE_NORMALISE=off|validate|mirror|exact
                        How to handle overlay paths whose corresponding src/
                        files moved or disappeared between source release branches.
                        Default: exact.
  CONTINUE=0|1          Continue a paused import after conflicts are resolved.
  ABORT=0|1             Remove paused import state without moving refs.
  ABORT_ALL=0|1         Remove all paused import-changes operations without
                        moving refs.
  OP_ID=<id>            Paused operation ID for CONTINUE=1 or ABORT=1.
  WRITE=0|1             Required before refs or tags are created or advanced.
  V=0|1                 Print delegated git operations.

This target is for changes developed on materialised source/cache/** branches,
legacy source branches, or other broader trees. Use import-unofficial-commits
instead when FROM_REF is already a topic branch based on source/unofficial/current.

Dry-run mode still applies the extracted patch in scratch trees for every
target and never moves refs or tags. If the dry run succeeds, those scratch
trees are removed. If it conflicts, scratch state is kept under .cache for
resolution. Resolve and stage conflicted files there, run CONTINUE=1 without
WRITE=1 to validate candidate commits, then add WRITE=1 only for the final
guarded ref/tag update.
"""


def progress(message: str) -> None:
    print(f"[import-changes] {message}", file=sys.stderr, flush=True)


def terminal_width() -> int:
    return max(80, shutil.get_terminal_size(fallback=(80, 24)).columns)


def append_wrapped(lines: list[str], text: str, *, indent: str = "") -> None:
    lines.extend(textwrap.wrap(text, width=terminal_width(), initial_indent=indent, subsequent_indent=indent) or [indent])


STATUS_LEGEND = {
    "A": "added",
    "C": "copied",
    "D": "deleted",
    "M": "modified",
    "R": "renamed",
    "T": "type changed",
    "U": "unmerged",
    "??": "untracked",
}


def status_codes(lines: list[str], *, porcelain: bool = False) -> list[str]:
    codes: set[str] = set()
    for line in lines:
        if not line:
            continue
        if porcelain:
            prefix = line[:2]
            if prefix == "??":
                codes.add("??")
            else:
                codes.update(ch for ch in prefix if ch != " ")
            continue
        code = line.split("\t", 1)[0]
        if code:
            codes.add(code[0])
    return sorted(codes, key=lambda value: ("~" if value == "??" else value))


def append_status_legend(lines: list[str], status_lines: list[str], *, indent: str, porcelain: bool = False) -> None:
    codes = status_codes(status_lines, porcelain=porcelain)
    if not codes:
        return
    meanings = ", ".join(f"{code}={STATUS_LEGEND.get(code, 'see git status')}" for code in codes)
    lines.append(f"{indent}status legend: {meanings}.")
    lines.append("")


TRAILING_WHITESPACE_RE = re.compile(r"^(?P<path>.+):(?P<line>[0-9]+): trailing whitespace\.$")


def format_apply_output(detail: str, *, limit: int) -> list[str]:
    """Make git-apply diagnostics clearer for conflict reports."""

    raw_lines = detail.splitlines()
    formatted: list[str] = []
    index = 0
    while index < len(raw_lines):
        line = raw_lines[index]
        match = TRAILING_WHITESPACE_RE.match(line)
        if match:
            formatted.append(
                f"{match.group('path')}:{match.group('line')}: warning: trailing whitespace in patch input"
            )
            if index + 1 < len(raw_lines):
                formatted.append(f"line content: {raw_lines[index + 1]!r}")
                index += 2
                continue
        else:
            formatted.append(line)
        index += 1
    if len(formatted) > limit:
        return formatted[:limit] + [f"... {len(formatted) - limit} more git-apply output line(s) omitted"]
    return formatted


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--base-ref", default=os.environ.get("BASE_REF", ""))
    p.add_argument("--source-unofficial-ref", default=os.environ.get("SOURCE_UNOFFICIAL_REF", CURRENT_REF))
    p.add_argument("--propagate-release-branches", dest="propagate_release_branches", default=os.environ.get("PROPAGATE_RELEASE_BRANCHES", "none"))
    p.add_argument("--propagate-checkpoints", dest="propagate_checkpoints", default=os.environ.get("PROPAGATE_CHECKPOINTS", ""))
    p.add_argument("--update-release-tags", dest="update_release_tags", default=os.environ.get("UPDATE_RELEASE_TAGS", "0"))
    p.add_argument("--commit-message", default=os.environ.get("COMMIT_MESSAGE", ""))
    p.add_argument("--commit-message-file", default=os.environ.get("COMMIT_MESSAGE_FILE", ""))
    p.add_argument("--signoff", default=os.environ.get("SIGNOFF", "0"))
    p.add_argument("--source-lifecycle-normalise", default=os.environ.get("SOURCE_LIFECYCLE_NORMALISE", "exact"))
    p.add_argument("--continue-import", default=os.environ.get("CONTINUE", "0"))
    p.add_argument("--abort", default=os.environ.get("ABORT", "0"))
    p.add_argument("--abort-all", default=os.environ.get("ABORT_ALL", "0"))
    p.add_argument("--op-id", default=os.environ.get("OP_ID", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def abort_all_operations(repo: Path) -> None:
    root = operations_root(repo, OP_NAMESPACE)
    op_dirs = sorted(path for path in root.iterdir() if path.is_dir() and (path / "state.json").exists()) if root.exists() else []
    if not op_dirs:
        print("no paused import-changes operations exist")
        return
    for op_dir in op_dirs:
        abort_operation(op_dir, "import-changes")


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


def branch_heads(repo: Path) -> list[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", "refs/heads", check=False)
    if result.returncode != 0:
        return []
    return [line for line in result.stdout.splitlines() if line]


def containing_unofficial_refs(repo: Path, from_ref: str) -> list[str]:
    from_oid = rev_parse(repo, from_ref)
    refs = []
    for ref in for_each_ref(repo, "source/unofficial"):
        oid = rev_parse(repo, ref)
        if oid == from_oid or is_ancestor(repo, from_oid, oid):
            refs.append(ref)
    return sorted(refs)


def reject_already_integrated_source(repo: Path, from_ref: str) -> None:
    containing = containing_unofficial_refs(repo, from_ref)
    if not containing:
        return
    shown = "\n".join(f"  - {ref}" for ref in containing[:12])
    extra = "" if len(containing) <= 12 else f"\n  ... and {len(containing) - 12} more"
    raise ReconstructionError(
        "FROM_REF is already contained by retained source/unofficial ref(s), so automatic base "
        "inference will not fall back to an unrelated legacy branch and create a large aggregate diff:\n"
        f"{shown}{extra}\n\n"
        "If this is intentional, re-run with an explicit BASE_REF naming the source tree immediately "
        "before the focused change. If you are propagating source/unofficial/current itself, use "
        "make propagate-release-branches or pass BASE_REF explicitly."
    )


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
            (int(record["count"]), oid, sorted(set(record["labels"])))
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

    reject_already_integrated_source(repo, from_ref)

    broader_base = infer_broader_base(repo, from_ref, broader_base_refs(repo, from_ref))
    if broader_base:
        return broader_base

    raise ReconstructionError(
        "could not infer BASE_REF. Re-run with BASE_REF=<source tree before the intended change>."
    )


def base_extraction_notes(repo: Path, base_oid: str, targets: list[dict[str, Any]]) -> list[str]:
    non_ancestor_targets = [
        str(target["ref"])
        for target in targets
        if not is_ancestor(repo, base_oid, rev_parse(repo, str(target["ref"])))
    ]
    if not non_ancestor_targets:
        return []

    if len(non_ancestor_targets) == len(targets):
        target_summary = "any update target"
    elif len(non_ancestor_targets) == 1:
        target_summary = non_ancestor_targets[0]
    else:
        shown = ", ".join(non_ancestor_targets[:3])
        extra = "" if len(non_ancestor_targets) <= 3 else f", and {len(non_ancestor_targets) - 3} more"
        target_summary = f"{shown}{extra}"

    return [
        "BASE_REF is the patch-extraction base, not the destination branch. "
        f"It is not an ancestor of {target_summary}; this is expected when applying "
        "a small patch from a branch with different history to the target branch. "
        "Only the changed paths listed below are applied to the update targets."
    ]


def changed_files(repo: Path, base_oid: str, from_ref: str) -> list[str]:
    result = git(repo, "diff", "--name-status", f"{base_oid}..{from_ref}")
    return [line for line in result.stdout.splitlines() if line]


def write_patch(repo: Path, base_oid: str, from_ref: str, patch_path: Path) -> list[str]:
    changes = changed_files(repo, base_oid, from_ref)
    if not changes:
        raise ReconstructionError("change diff is empty; FROM_REF contains no tree changes after BASE_REF")
    patch_path.parent.mkdir(parents=True, exist_ok=True)
    patch = subprocess.run(
        ["git", "-C", str(repo), "diff", "--binary", "--full-index", f"{base_oid}..{from_ref}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if patch.returncode != 0:
        raise ReconstructionError((patch.stderr or patch.stdout).decode("utf-8", errors="replace").strip())
    patch_path.write_bytes(patch.stdout)
    return changes


def staged_changes(repo: Path) -> bool:
    return git(repo, "diff", "--cached", "--quiet", check=False).returncode != 0


def dirty_paths(repo: Path) -> list[str]:
    result = git(repo, "status", "--porcelain", check=False)
    return [line for line in result.stdout.splitlines() if line]


def reject_paths(repo: Path) -> list[str]:
    result = git(repo, "ls-files", "--others", "--exclude-standard", "--", "*.rej", check=False)
    return sorted(line for line in result.stdout.splitlines() if line)


def reject_artifact_paths(repo: Path) -> list[str]:
    tracked = git(repo, "ls-files", "--", "*.rej", check=False)
    return sorted(set(reject_paths(repo) + [line for line in tracked.stdout.splitlines() if line]))


def changed_paths(changes: list[str]) -> list[str]:
    paths: list[str] = []
    for change in changes:
        parts = change.split("\t")
        if len(parts) < 2:
            continue
        if parts[0].startswith(("R", "C")) and len(parts) >= 3:
            paths.extend(parts[1:3])
        else:
            paths.extend(parts[1:])
    return sorted(set(paths))


def commit_message_from_ref(repo: Path, from_ref: str) -> str:
    message = git(repo, "log", "-1", "--format=%B", from_ref).stdout.rstrip("\n")
    if not message.strip():
        raise ReconstructionError(f"FROM_REF has an empty commit message: {from_ref}")
    return message


def commit_message_file(repo: Path, message_file: str) -> Path:
    path = Path(message_file)
    if not path.is_absolute():
        path = repo / path
    if not path.is_file():
        raise ReconstructionError(f"COMMIT_MESSAGE_FILE does not exist: {message_file}")
    return path


def explicit_message_parts(message: str) -> list[str]:
    decoded = message.replace("\\n", "\n")
    parts = [part for part in decoded.splitlines() if part]
    if not parts:
        raise ReconstructionError("COMMIT_MESSAGE is empty")
    return parts


def resolved_commit_message(repo: Path, args: argparse.Namespace) -> dict[str, Any]:
    if args.commit_message and args.commit_message_file:
        raise ReconstructionError("set only one of COMMIT_MESSAGE or COMMIT_MESSAGE_FILE")

    if args.commit_message_file:
        path = commit_message_file(repo, args.commit_message_file)
        message = path.read_text(encoding="utf-8").rstrip("\n")
        if not message.strip():
            raise ReconstructionError(f"COMMIT_MESSAGE_FILE is empty: {args.commit_message_file}")
        return {
            "message": message,
            "message_parts": [],
            "message_source": "file",
        }

    if args.commit_message:
        parts = explicit_message_parts(args.commit_message)
        return {
            "message": "\n".join(parts),
            "message_parts": parts,
            "message_source": "explicit",
        }

    return {
        "message": commit_message_from_ref(repo, args.from_ref),
        "message_parts": [],
        "message_source": "from-ref",
    }


def git_commit(repo: Path, message: str, message_parts: list[str], signoff: bool) -> None:
    cmd = ["git", "-C", str(repo), "commit"]
    if signoff:
        cmd.append("-s")
    if message_parts:
        for part in message_parts:
            cmd.extend(["-m", part])
        result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    else:
        cmd.extend(["-F", "-"])
        result = subprocess.run(
            cmd,
            input=message if message.endswith("\n") else f"{message}\n",
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    if result.returncode != 0:
        raise ReconstructionError((result.stderr or result.stdout).strip() or "git commit failed")


def commit_scratch(repo: Path, state: dict[str, Any]) -> str:
    unresolved = unmerged_paths(repo)
    if unresolved:
        raise ReconstructionError("import still has unresolved conflicts:\n" + "\n".join(f"  - {path}" for path in unresolved))
    rejects = reject_artifact_paths(repo)
    if rejects:
        raise ReconstructionError(
            "reject files remain in the scratch tree. Apply or explicitly discard their hunks, "
            "remove the .rej files, stage the resolved files, and then continue:\n"
            + "\n".join(f"  - {path}" for path in rejects)
        )
    enforce_source_tree_policy(repo, index=True, label="import scratch index")
    if not staged_changes(repo):
        dirty = dirty_paths(repo)
        if dirty:
            raise ReconstructionError(
                "resolved files are not staged in the scratch tree. Run git add for the resolved files, then continue the import."
            )
        raise ReconstructionError("import patch produced no staged changes")
    git_commit(repo, state["message"], state.get("message_parts", []), truthy(str(state.get("signoff", "0"))))
    return rev_parse(repo, "HEAD")


def normalise_target(repo: Path, target: dict[str, Any], state: dict[str, Any], verbose: bool) -> None:
    normalise_overlay_lifecycle(
        Path(target["scratch"]),
        source_repo=repo,
        from_ref=state["from_oid"],
        to_ref=target["old_oid"],
        paths=state.get("changed_overlay_paths", []),
        mode=state.get("source_lifecycle_normalise", "exact"),
        verbose=verbose,
    )


def apply_patch_to_target(repo: Path, target: dict[str, Any], state: dict[str, Any], patch_path: Path, verbose: bool) -> bool:
    scratch = Path(target["scratch"])
    if target["old_oid"] == state["from_oid"]:
        target["status"] = "ready"
        target["candidate_oid"] = target["old_oid"]
        target["already_applied"] = True
        return True
    result = subprocess.run(
        ["git", "-C", str(scratch), "apply", "--3way", "--index", "--binary", str(patch_path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode == 0:
        normalise_target(repo, target, state, verbose)
        target["status"] = "ready"
        if not staged_changes(scratch) and not dirty_paths(scratch):
            target["candidate_oid"] = target["old_oid"]
            return True
        target["candidate_oid"] = commit_scratch(scratch, state)
        return True
    conflicts = unmerged_paths(scratch)
    if conflicts:
        target["status"] = "conflict"
        target["conflict_paths"] = conflicts
        target["apply_stdout"] = result.stdout
        target["apply_stderr"] = result.stderr
        return False

    # Some git-apply failures cannot form index conflict stages and otherwise
    # leave a clean tree. Replay with rejects so the user has concrete files to
    # inspect and resolve from the dry-run scratch tree.
    git(scratch, "reset", "--hard", target["old_oid"], capture=not verbose)
    git(scratch, "clean", "-fd", capture=not verbose)
    reject_result = subprocess.run(
        ["git", "-C", str(scratch), "apply", "--reject", "--binary", str(patch_path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    reject_files = reject_paths(scratch)
    fallback_dirty = dirty_paths(scratch)
    if not reject_files and (reject_result.returncode == 0 or fallback_dirty):
        normalise_target(repo, target, state, verbose)
        target["status"] = "ready"
        if not staged_changes(scratch) and not dirty_paths(scratch):
            target["candidate_oid"] = target["old_oid"]
            return True
        git(scratch, "add", "-A", capture=not verbose)
        target["candidate_oid"] = commit_scratch(scratch, state)
        return True
    target["status"] = "conflict"
    target["conflict_paths"] = changed_paths(state.get("changes", []))
    target["reject_paths"] = reject_files
    target["dirty_paths"] = dirty_paths(scratch)
    target["manual_patch_path"] = str(patch_path)
    target["apply_stdout"] = result.stdout
    target["apply_stderr"] = result.stderr
    target["reject_apply_stdout"] = reject_result.stdout
    target["reject_apply_stderr"] = reject_result.stderr
    return False


def attach_conflict_report(op_dir: Path, target: dict[str, Any]) -> None:
    scratch = target.get("scratch")
    if not scratch:
        return
    report_path = op_dir / "reports" / f"{safe_name(target['ref'])}.txt"
    write_conflict_report(Path(scratch), report_path, paths=target.get("conflict_paths") or None)
    target["conflict_report_path"] = str(report_path)


def build_targets(repo: Path, args: argparse.Namespace) -> list[dict[str, Any]]:
    require_unofficial_target(args.source_unofficial_ref)
    if args.propagate_checkpoints:
        raise ReconstructionError(
            "PROPAGATE_CHECKPOINTS was renamed to PROPAGATE_RELEASE_BRANCHES; "
            "use PROPAGATE_RELEASE_BRANCHES=all."
        )
    propagate = args.propagate_release_branches.strip().lower() or "none"
    if propagate not in {"none", "0", "false", "all"}:
        raise ReconstructionError("PROPAGATE_RELEASE_BRANCHES must be none or all")
    if propagate == "all" and args.source_unofficial_ref != CURRENT_REF:
        raise ReconstructionError("PROPAGATE_RELEASE_BRANCHES=all updates source/unofficial/current and all release branches; do not set SOURCE_UNOFFICIAL_REF")
    if propagate != "all" and truthy(args.update_release_tags) and args.source_unofficial_ref == CURRENT_REF:
        raise ReconstructionError(
            "UPDATE_RELEASE_TAGS=1 requires PROPAGATE_RELEASE_BRANCHES=all so tags only move after every "
            "requested release-branch import succeeds. For the safer staged workflow, omit "
            "UPDATE_RELEASE_TAGS, test the propagated branches, then run make update-release-tags."
        )

    refs = [args.source_unofficial_ref]
    if propagate == "all":
        refs.extend(release_branch_targets(repo))

    targets: list[dict[str, Any]] = []
    for ref in refs:
        target: dict[str, Any] = {
            "ref": ref,
            "old_oid": rev_parse(repo, ref),
            "status": "pending",
        }
        if truthy(args.update_release_tags) and (propagate != "all" or ref != args.source_unofficial_ref):
            tag = unofficial_release_tag_for_branch(ref)
            target["tag"] = tag
            target["tag_old_oid"] = ref_oid(repo, tag, tag=True) or ZERO_OID
        targets.append(target)
    return targets


def pause_message(op_id: str, targets: list[dict[str, Any]]) -> str:
    lines = ["Import paused due to conflicts.", "", "Resolve conflicts in:"]
    for target in targets:
        if target.get("status") == "conflict":
            lines.append("")
            lines.append(f"  {target['scratch']}")
            conflict_paths = target.get("conflict_paths") or []
            if conflict_paths:
                lines.append("")
                lines.append("  conflicting file(s):")
                lines.extend(f"    {path}" for path in conflict_paths)
            reject_paths_value = target.get("reject_paths") or []
            if reject_paths_value:
                lines.append("")
                lines.append("  reject file(s):")
                lines.extend(f"    {path}" for path in reject_paths_value)
            dirty_paths_value = target.get("dirty_paths") or []
            if dirty_paths_value:
                lines.append("")
                lines.append("  scratch status:")
                lines.extend(f"    {path}" for path in dirty_paths_value[:12])
                append_status_legend(lines, dirty_paths_value, indent="  ", porcelain=True)
            manual_patch_path = target.get("manual_patch_path")
            if manual_patch_path:
                lines.append("")
                lines.append(f"  extracted patch: {manual_patch_path}")
            resolution_error = target.get("resolution_error")
            if resolution_error:
                lines.append("")
                lines.append("  current issue:")
                lines.extend(f"    {line}" for line in resolution_error.splitlines()[:8])
            detail = (target.get("apply_stderr") or target.get("apply_stdout") or "").strip()
            if detail:
                lines.append("")
                lines.append("  apply output (Git warnings/errors; whitespace-warning line content is quoted):")
                lines.extend(f"    {line}" for line in format_apply_output(detail, limit=12))
            reject_detail = (target.get("reject_apply_stderr") or target.get("reject_apply_stdout") or "").strip()
            if reject_detail:
                lines.append("")
                lines.append("  reject apply output (Git warnings/errors; whitespace-warning line content is quoted):")
                lines.extend(f"    {line}" for line in format_apply_output(reject_detail, limit=12))
            conflict_report_path = target.get("conflict_report_path")
            if conflict_report_path:
                lines.append("")
                lines.append(f"  symlink-aware conflict report: {conflict_report_path}")
    lines.append("")
    append_wrapped(lines, "If Git created .rej files, use them to apply the missing hunks. If it could not create .rej files, use the printed extracted patch as the manual source of truth. Remove .rej files when they are no longer needed, stage all resolved files with git add, and continue.")
    lines.append("")
    append_wrapped(lines, "For mode conflicts involving symlinks, inspect the symlink-aware report or run:")
    lines.extend(
        [
            f"  make inspect-import-conflicts OP_ID={op_id}",
            "",
            "Then validate the resolved candidates without moving refs:",
            f"  make import-changes CONTINUE=1 OP_ID={op_id}",
            "",
            "When the candidates are ready, move refs and tags deliberately:",
            f"  make import-changes CONTINUE=1 OP_ID={op_id} WRITE=1",
            "",
            "Or abort:",
            f"  make import-changes ABORT=1 OP_ID={op_id}",
        ]
    )
    return "\n".join(lines)


def prepare_operation(repo: Path, args: argparse.Namespace, verbose: bool) -> tuple[Path, dict[str, Any], list[dict[str, Any]]]:
    progress("inferring base ref")
    base_ref, base_oid = infer_base(repo, args.from_ref, args.base_ref, args.source_unofficial_ref)
    progress(f"using base {base_ref} ({base_oid[:12]})")
    from_oid = rev_parse(repo, args.from_ref)
    progress("preparing update targets")
    targets = build_targets(repo, args)
    for target in targets:
        ensure_target_not_checked_out_dirty(repo, target["ref"])

    op_id = args.op_id or make_op_id(args.from_ref)
    op_dir = operation_path(repo, OP_NAMESPACE, op_id)
    if op_dir.exists():
        raise ReconstructionError(f"import operation already exists: {op_id}")
    op_dir.mkdir(parents=True)
    patch_path = op_dir / "change.patch"
    progress("extracting change patch")
    changes = write_patch(repo, base_oid, args.from_ref, patch_path)
    commit_message = resolved_commit_message(repo, args)
    lifecycle_mode = normalise_mode(args.source_lifecycle_normalise)
    state: dict[str, Any] = {
        "op_id": op_id,
        "from_ref": args.from_ref,
        "from_oid": from_oid,
        "base_ref": base_ref,
        "base_oid": base_oid,
        "base_notes": base_extraction_notes(repo, base_oid, targets),
        "patch_path": str(patch_path),
        "changes": changes,
        "changed_overlay_paths": changed_overlay_paths_from_name_status(changes),
        "source_lifecycle_normalise": lifecycle_mode,
        "message": commit_message["message"],
        "message_parts": commit_message["message_parts"],
        "message_source": commit_message["message_source"],
        "signoff": truthy(args.signoff),
        "requested": {
            "base_ref": args.base_ref,
            "commit_message": args.commit_message,
            "commit_message_file": args.commit_message_file,
            "propagate_release_branches": args.propagate_release_branches,
            "signoff": args.signoff,
            "source_lifecycle_normalise": args.source_lifecycle_normalise,
            "source_unofficial_ref": args.source_unofficial_ref,
            "update_release_tags": args.update_release_tags,
        },
        "targets": targets,
    }

    paused: list[dict[str, Any]] = []
    try:
        progress(f"applying patch to {len(targets)} target(s)")
        for target in targets:
            progress(f"preparing scratch tree for {target['ref']}")
            target["scratch"] = str(clone_scratch(repo, op_dir, target["ref"], verbose))
            progress(f"applying patch to {target['ref']}")
            if not apply_patch_to_target(repo, target, state, patch_path, verbose):
                attach_conflict_report(op_dir, target)
                paused.append(target)
            save_state(op_dir, state)
    except Exception:
        remove_operation_state(op_dir, ignore_errors=True)
        raise
    return op_dir, state, paused


def finalise(repo: Path, op_dir: Path, state: dict[str, Any], verbose: bool) -> None:
    not_ready = [target["ref"] for target in state["targets"] if target.get("status") != "ready"]
    if not_ready:
        raise ReconstructionError("cannot finalise; target is not ready:\n" + "\n".join(f"  - {ref}" for ref in not_ready))
    message_problems = scan_commit_message_for_legacy_branch("import", state["message"])
    if message_problems:
        raise ReconstructionError(
            "import commit message failed identity integrity check:\n"
            + "\n".join(f"  - {problem}" for problem in message_problems)
            + "\n\nUse COMMIT_MESSAGE=<text> or COMMIT_MESSAGE_FILE=<path> with a corrected message."
        )
    progress("fetching candidate objects")
    fetch_candidate_objects(repo, state, verbose)
    updates: list[tuple[str, str, str]] = []
    for target in state["targets"]:
        updates.append((branch_to_ref(target["ref"]), target["candidate_oid"], target["old_oid"]))
        if target.get("tag"):
            updates.append((full_tag_ref(target["tag"]), target["candidate_oid"], target.get("tag_old_oid") or ZERO_OID))
    progress("updating refs")
    transaction_update_refs(repo, updates)
    for target in state["targets"]:
        if target["ref"] == CURRENT_REF and target.get("candidate_oid") != target.get("old_oid"):
            write_current_import_receipt(
                repo,
                tool="import-changes",
                from_ref=str(state.get("from_ref", "")),
                base_ref=str(state.get("base_ref", "")),
                base_oid=str(state.get("base_oid", "")),
                old_oid=str(target.get("old_oid", "")),
                new_oid=str(target.get("candidate_oid", "")),
            )
    print("updated unofficial refs:")
    for full_ref, new_oid, _old_oid in updates:
        print(f"  {full_ref} -> {new_oid}")
    remove_operation_state(op_dir)


def ready_message(state: dict[str, Any]) -> str:
    op_id = state["op_id"]
    lines = [
        "import candidates are ready; no refs or tags were moved.",
        "",
        "Candidate scratch trees:",
    ]
    for target in state["targets"]:
        candidate = target.get("candidate_oid", target.get("old_oid", ""))
        lines.append(f"  - {target['ref']}: {candidate}")
        scratch = target.get("scratch")
        if scratch:
            lines.append(f"    scratch: {scratch}")
        if target.get("tag"):
            lines.append(f"    tag: {target['tag']}")
    lines.extend(
        [
            "",
            "Review the scratch trees if needed. When ready, move refs and tags:",
            f"  make import-changes CONTINUE=1 OP_ID={op_id} WRITE=1",
            "",
            "Or abort without moving refs:",
            f"  make import-changes ABORT=1 OP_ID={op_id}",
        ]
    )
    return "\n".join(lines)


def finalise_or_report_ready(repo: Path, op_dir: Path, state: dict[str, Any], verbose: bool, write: bool) -> None:
    not_ready = [target["ref"] for target in state["targets"] if target.get("status") != "ready"]
    if not_ready:
        raise ReconstructionError("cannot finalise; target is not ready:\n" + "\n".join(f"  - {ref}" for ref in not_ready))
    save_state(op_dir, state)
    if not write:
        print(ready_message(state))
        return
    finalise(repo, op_dir, state, verbose)


def normalised_requested_option(name: str, value: str) -> str:
    if name == "source_unofficial_ref":
        return value or CURRENT_REF
    if name == "propagate_release_branches":
        return (value or "none").lower()
    if name in {"signoff", "update_release_tags"}:
        return "1" if truthy(value) else "0"
    if name == "source_lifecycle_normalise":
        return value or "exact"
    return value or ""


def validate_continue_arguments(args: argparse.Namespace, state: dict[str, Any]) -> None:
    """Reject option changes that a paused operation cannot honour."""

    requested = state.get("requested", {})
    comparisons = {
        "BASE_REF": ("base_ref", str(requested.get("base_ref") or "")),
        "COMMIT_MESSAGE": ("commit_message", str(requested.get("commit_message") or "")),
        "COMMIT_MESSAGE_FILE": ("commit_message_file", str(requested.get("commit_message_file") or "")),
        "FROM_REF": ("from_ref", str(state.get("from_ref") or "")),
        "PROPAGATE_RELEASE_BRANCHES": ("propagate_release_branches", str(requested.get("propagate_release_branches") or "")),
        "SIGNOFF": ("signoff", str(requested.get("signoff") or "")),
        "SOURCE_LIFECYCLE_NORMALISE": (
            "source_lifecycle_normalise",
            str(requested.get("source_lifecycle_normalise") or ""),
        ),
        "SOURCE_UNOFFICIAL_REF": ("source_unofficial_ref", str(requested.get("source_unofficial_ref") or "")),
        "UPDATE_RELEASE_TAGS": ("update_release_tags", str(requested.get("update_release_tags") or "")),
    }
    mismatches: list[str] = []
    for env_name, (state_name, recorded) in comparisons.items():
        supplied = os.environ.get(env_name, "")
        if supplied == "":
            continue
        if normalised_requested_option(state_name, supplied) == normalised_requested_option(state_name, recorded):
            continue
        mismatches.append(
            f"  - {env_name}={supplied!r}; operation has {normalised_requested_option(state_name, recorded)!r}"
        )
    if mismatches:
        raise ReconstructionError(
            "CONTINUE=1 uses the targets and options captured when the import operation was created.\n"
            "The supplied option(s) would be ignored, so the import has been stopped:\n"
            + "\n".join(mismatches)
            + "\n\nAbort this operation and start a new dry run with the desired options, "
            "or continue without changing them."
        )


def continue_operation(repo: Path, op_dir: Path, args: argparse.Namespace, verbose: bool, write: bool) -> None:
    progress(f"continuing paused operation {op_dir.name}")
    state = load_state(op_dir)
    validate_continue_arguments(args, state)
    paused: list[dict[str, Any]] = []
    for target in state["targets"]:
        if target.get("status") != "conflict":
            continue
        scratch = Path(target["scratch"])
        try:
            normalise_target(repo, target, state, verbose)
            if not staged_changes(scratch) and not dirty_paths(scratch):
                raise ReconstructionError(
                    "conflicted scratch tree has no staged or unstaged changes. "
                    "Apply or resolve the intended change in the printed scratch tree, "
                    "stage the resolved files with git add, and then continue."
                )
            target["candidate_oid"] = commit_scratch(scratch, state)
            target["status"] = "ready"
        except ReconstructionError as exc:
            target["resolution_error"] = str(exc)
            paused.append(target)
    save_state(op_dir, state)
    if paused:
        raise ReconstructionError(pause_message(state["op_id"], paused))
    finalise_or_report_ready(repo, op_dir, state, verbose, write)


def print_dry_run(state: dict[str, Any]) -> None:
    print("dry run; no refs or tags will be moved")
    print(f"  base: {state['base_ref']} ({state['base_oid']})")
    print(f"  from: {state['from_ref']} ({state['from_oid']})")
    for note in state.get("base_notes", []):
        note_lines: list[str] = []
        append_wrapped(note_lines, f"note: {note}", indent="  ")
        for line in note_lines:
            print(line)
    print()
    print("  changed paths:")
    for change in state["changes"]:
        print(f"    {change}")
    legend_lines: list[str] = []
    append_status_legend(legend_lines, state["changes"], indent="  ")
    for line in legend_lines:
        print(line)
    print()
    print(f"  source lifecycle normalise: {state.get('source_lifecycle_normalise', 'exact')}")
    if state.get("changed_overlay_paths"):
        print()
        print("  changed overlay paths:")
        for path in state["changed_overlay_paths"]:
            print(f"    {path}")
    print()
    print(f"  commit message source: {state.get('message_source', 'unknown')}")
    print("  commit message:")
    for line in str(state.get("message", "")).splitlines() or [""]:
        print(f"    {line}")
    if truthy(str(state.get("signoff", "0"))):
        print("  signoff: yes")
    print()
    print("  update targets:")
    for target in state["targets"]:
        line = f"    {target['ref']}"
        if target.get("tag"):
            line += f" and {target['tag']}"
        print(line)
    print()
    sys.stdout.flush()


def print_dry_run_success(state: dict[str, Any]) -> None:
    print()
    print("Dry-run succeeded. To apply this change permanently, run:")
    print("  " + " ".join(write_command(state)))
    print()
    print("After the ref update succeeds, run at least:")
    print("  make test")
    if [target.get("ref") for target in state.get("targets", [])] == [CURRENT_REF]:
        print()
        print("Then test the updated source/unofficial/current source target.")
        print("If the change should apply to every supported EDK2 release, run:")
        print("  make propagate-release-branches")
        print("When that dry run is clean, move the release branches:")
        print("  make propagate-release-branches WRITE=1")
        print("After release-branch testing succeeds, update the release tags:")
        print("  make update-release-tags")
        print("  make update-release-tags WRITE=1")
    sys.stdout.flush()


def make_arg(name: str, value: str) -> str:
    return shlex.quote(f"{name}={value}")


def write_command(state: dict[str, Any]) -> list[str]:
    command = ["make", "import-changes"]
    command.append(make_arg("FROM_REF", state["from_ref"]))

    requested = state.get("requested", {})
    base_ref = str(requested.get("base_ref") or "")
    if base_ref:
        command.append(make_arg("BASE_REF", base_ref))

    source_ref = str(requested.get("source_unofficial_ref") or CURRENT_REF)
    if source_ref != CURRENT_REF:
        command.append(make_arg("SOURCE_UNOFFICIAL_REF", source_ref))

    propagate = str(requested.get("propagate_release_branches") or "none")
    if propagate not in {"", "none", "0", "false"}:
        command.append(make_arg("PROPAGATE_RELEASE_BRANCHES", propagate))

    update_tags = str(requested.get("update_release_tags") or "0")
    if truthy(update_tags):
        command.append(make_arg("UPDATE_RELEASE_TAGS", update_tags))

    lifecycle = str(requested.get("source_lifecycle_normalise") or "exact")
    if lifecycle != "exact":
        command.append(make_arg("SOURCE_LIFECYCLE_NORMALISE", lifecycle))

    commit_message = str(requested.get("commit_message") or "")
    if commit_message:
        command.append(make_arg("COMMIT_MESSAGE", commit_message))

    commit_message_file = str(requested.get("commit_message_file") or "")
    if commit_message_file:
        command.append(make_arg("COMMIT_MESSAGE_FILE", commit_message_file))

    signoff = str(requested.get("signoff") or "0")
    if truthy(signoff):
        command.append("SIGNOFF=1")

    command.append("WRITE=1")
    return command


def dry_run_conflict_message(state: dict[str, Any], paused: list[dict[str, Any]]) -> str:
    op_id = state["op_id"]
    lines = [
        "dry run detected conflicts while applying the extracted patch.",
        "No refs or tags were moved.",
        "",
        f"BASE_REF: {state['base_ref']} ({state['base_oid']})",
        f"FROM_REF: {state['from_ref']} ({state['from_oid']})",
    ]
    for note in state.get("base_notes", []):
        append_wrapped(lines, f"Note: {note}")
    lines.append("")
    append_wrapped(
        lines,
        "Scratch trees have been kept for conflict resolution under:",
    )
    lines.extend([f"  {operation_path(repo_root(Path(__file__)), OP_NAMESPACE, op_id)}", "", "Conflicting target(s):", ""])
    for target in paused:
        lines.append(f"  - {target['ref']}")
        scratch = target.get("scratch")
        if scratch:
            lines.append(f"    scratch: {scratch}")
        conflict_paths = target.get("conflict_paths") or []
        if conflict_paths:
            lines.append("")
            lines.append("    conflicting file(s):")
            lines.extend(f"      {path}" for path in conflict_paths)
        reject_paths_value = target.get("reject_paths") or []
        if reject_paths_value:
            lines.append("")
            lines.append("    reject file(s):")
            lines.extend(f"      {path}" for path in reject_paths_value)
        dirty_paths_value = target.get("dirty_paths") or []
        if dirty_paths_value:
            lines.append("")
            lines.append("    scratch status:")
            lines.extend(f"      {path}" for path in dirty_paths_value[:12])
            append_status_legend(lines, dirty_paths_value, indent="    ", porcelain=True)
        manual_patch_path = target.get("manual_patch_path")
        if manual_patch_path:
            lines.append("")
            lines.append(f"    extracted patch: {manual_patch_path}")
        detail = (target.get("apply_stderr") or target.get("apply_stdout") or "").strip()
        if detail:
            lines.append("")
            lines.append("    apply output (Git warnings/errors; whitespace-warning line content is quoted):")
            lines.extend(f"      {line}" for line in format_apply_output(detail, limit=8))
        reject_detail = (target.get("reject_apply_stderr") or target.get("reject_apply_stdout") or "").strip()
        if reject_detail:
            lines.append("")
            lines.append("    reject apply output (Git warnings/errors; whitespace-warning line content is quoted):")
            lines.extend(f"      {line}" for line in format_apply_output(reject_detail, limit=8))
        conflict_report_path = target.get("conflict_report_path")
        if conflict_report_path:
            lines.append("")
            lines.append(f"    symlink-aware conflict report: {conflict_report_path}")
        lines.append("")
    lines.append("")
    append_wrapped(
        lines,
        "Resolve each printed scratch tree. If .rej files were created, use them to apply the missing hunks. If Git could not create .rej files, use the printed extracted patch as the manual source of truth.",
    )
    lines.append("")
    append_wrapped(lines, "For mode conflicts involving symlinks, inspect the symlink-aware report or run:")
    lines.extend(
        [
            f"  make inspect-import-conflicts OP_ID={op_id}",
        ]
    )
    append_wrapped(
        lines,
        "Remove .rej files when they are no longer needed. Stage all resolved files with git add inside that scratch tree, then validate the resolved candidates:",
    )
    lines.extend(
        [
            f"  make import-changes CONTINUE=1 OP_ID={op_id}",
            "",
            "Only after that validation succeeds, move refs and tags:",
            f"  make import-changes CONTINUE=1 OP_ID={op_id} WRITE=1",
            "",
            "Or abort without moving refs:",
            f"  make import-changes ABORT=1 OP_ID={op_id}",
        ]
    )
    return "\n".join(lines)


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)

    if truthy(args.abort):
        progress("aborting paused operation")
        abort_operation(resolve_operation(repo, OP_NAMESPACE, "import-changes", args.op_id), "import-changes")
        return
    if truthy(args.abort_all):
        progress("aborting all paused operations")
        abort_all_operations(repo)
        return
    if truthy(args.continue_import):
        continue_operation(repo, resolve_operation(repo, OP_NAMESPACE, "import-changes", args.op_id), args, verbose, truthy(args.write))
        return

    if not args.from_ref:
        print(HELP)
        print("missing required variable(s): FROM_REF", file=sys.stderr)
        raise SystemExit(2)
    progress(f"starting import from {args.from_ref}")
    if not ref_exists(repo, args.from_ref):
        raise ReconstructionError(f"FROM_REF is unavailable locally: {args.from_ref}")
    if args.propagate_checkpoints:
        raise ReconstructionError(
            "PROPAGATE_CHECKPOINTS was renamed to PROPAGATE_RELEASE_BRANCHES; "
            "use PROPAGATE_RELEASE_BRANCHES=all."
        )

    if not truthy(args.write):
        progress("dry run: no refs or tags will be moved")
        op_dir, state, paused = prepare_operation(repo, args, verbose)
        print_dry_run(state)
        if paused:
            raise ReconstructionError(dry_run_conflict_message(state, paused))
        print_dry_run_success(state)
        remove_operation_state(op_dir)
        return

    op_dir, state, paused = prepare_operation(repo, args, verbose)
    if paused:
        raise ReconstructionError(pause_message(state["op_id"], paused))
    finalise(repo, op_dir, state, verbose)


if __name__ == "__main__":
    main_wrapper(main)
