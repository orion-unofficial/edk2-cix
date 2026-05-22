#!/usr/bin/env python3
"""Helpers for replaying source-tree deltas onto a newer EDK2 base."""

from __future__ import annotations

import subprocess
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    git,
    resolve_ref,
    resolve_ref_or_generated_cache,
    safe_name,
    temp_dir,
    temp_path,
    tree_id,
)
from render_release_branch import materialise_submodules


def commit_index(repo: Path, worktree: Path, message: str) -> str:
    tree = git(worktree, "write-tree").stdout.strip()
    return git(repo, "commit-tree", tree, "-m", message).stdout.strip()


def commit_tree(repo: Path, tree: str, message: str) -> str:
    return git(repo, "commit-tree", tree, "-m", message).stdout.strip()


def materialised_base_commit(repo: Path, base_ref: str, *, label: str, verbose: bool) -> str:
    """Return a commit for an EDK2 base with nested gitlinks flattened."""

    base_commit = resolve_ref_or_generated_cache(repo, base_ref)
    with temp_dir(repo, f"materialise-{safe_name(label)}-") as tmp:
        worktree = Path(tmp) / "worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), base_commit, capture=not verbose)
        try:
            materialise_submodules(repo, worktree, label, verbose)
            status = git(worktree, "status", "--porcelain").stdout.strip()
            if not status:
                return base_commit
            return commit_index(repo, worktree, f"base: materialise submodules for {base_ref}")
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False, capture=True)


def compact_apply_detail(detail: str) -> str:
    lines = detail.splitlines()
    compact: list[str] = []
    fallback_count = 0
    for line in lines:
        if line == "Falling back to direct application...":
            fallback_count += 1
            continue
        if fallback_count:
            compact.append(f"Falling back to direct application... ({fallback_count} time(s))")
            fallback_count = 0
        compact.append(line)
    if fallback_count:
        compact.append(f"Falling back to direct application... ({fallback_count} time(s))")
    if len(compact) <= 80:
        return "\n".join(compact)
    return "\n".join([*compact[:40], "...", *compact[-40:]])


def conflicted_paths(merge_output: str) -> set[str]:
    paths: set[str] = set()
    for line in merge_output.splitlines():
        if "\t" not in line:
            continue
        meta, path = line.split("\t", 1)
        parts = meta.split()
        if len(parts) == 3 and parts[0] in {"100644", "100755", "120000"} and parts[2] in {"1", "2", "3"}:
            paths.add(path)
    return paths


def preserve_conflict_worktree(
    repo: Path,
    *,
    tree: str,
    label: str,
    merge_output: str,
    source_ref: str,
    new_base_ref: str,
    verbose: bool,
) -> Path:
    scratch = temp_path(repo, f"port-{safe_name(label)}-conflict-")
    worktree = scratch / "worktree"
    paths = sorted(conflicted_paths(merge_output))
    conflict_commit = commit_tree(
        repo,
        tree,
        f"source-port: conflict tree for {label}\n\n"
        f"Source-Port-Input: {source_ref}\n"
        f"Source-Port-New-Base: {new_base_ref}\n",
    )
    git(repo, "worktree", "add", "--detach", str(worktree), conflict_commit, capture=not verbose)
    notes = [
        f"# Source Port Conflict: {label}",
        "",
        "Git could not merge the old source tree onto the new base automatically.",
        "Resolve the conflict markers in the worktree, commit the result, then rerun the original integration command with:",
        "",
        f"    REF=$(git -C {worktree} rev-parse HEAD)",
        "    MATERIALISE=0",
        "",
        "Conflicted paths:",
        *[f"  - {path}" for path in paths],
        "",
        "Original merge-tree output:",
        "",
        "```",
        merge_output.strip(),
        "```",
        "",
    ]
    (scratch / "README.md").write_text("\n".join(notes), encoding="utf-8")
    return worktree


def merge_source_tree(
    repo: Path,
    old_base: str,
    new_base: str,
    source_ref: str,
    *,
    label: str,
    new_base_ref: str,
    verbose: bool,
) -> str:
    result = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "merge-tree",
            "--write-tree",
            f"--merge-base={old_base}",
            new_base,
            resolve_ref(repo, source_ref),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        tree = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
        worktree_note = ""
        if tree and git(repo, "cat-file", "-t", tree, check=False).stdout.strip() == "tree":
            if not conflicted_paths(result.stdout):
                return tree
            worktree = preserve_conflict_worktree(
                repo,
                tree=tree,
                label=label,
                merge_output=result.stdout,
                source_ref=source_ref,
                new_base_ref=new_base_ref,
                verbose=verbose,
            )
            worktree_note = (
                f"\n\nsource-port conflict worktree preserved at: {worktree}\n"
                "Resolve conflicts there, commit the result, and rerun with REF=<resolved-commit> MATERIALISE=0."
            )
        detail = compact_apply_detail((result.stderr or result.stdout or "unknown merge failure").strip())
        raise ReconstructionError(
            f"could not three-way merge source delta from {source_ref} onto {new_base}: {detail}{worktree_note}"
        )
    tree = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
    if not tree:
        raise ReconstructionError(f"git merge-tree produced no tree for source delta from {source_ref}")
    kind = git(repo, "cat-file", "-t", tree).stdout.strip()
    if kind != "tree":
        raise ReconstructionError(f"git merge-tree produced a {kind}, not a tree, for source delta from {source_ref}")
    return tree


def apply_source_delta_to_base(
    repo: Path,
    *,
    old_base_ref: str,
    source_ref: str,
    new_base_ref: str,
    message: str,
    label: str,
    verbose: bool,
) -> str:
    """Replay the source_ref delta from old_base_ref onto new_base_ref."""

    old_base = materialised_base_commit(repo, old_base_ref, label=f"{label}-old-base", verbose=verbose)
    new_base = materialised_base_commit(repo, new_base_ref, label=f"{label}-new-base", verbose=verbose)
    tree = merge_source_tree(
        repo,
        old_base,
        new_base,
        source_ref,
        label=label,
        new_base_ref=new_base_ref,
        verbose=verbose,
    )
    if tree == tree_id(repo, new_base):
        return new_base
    return commit_tree(repo, tree, message)


def same_tree(repo: Path, left: str, right: str) -> bool:
    return tree_id(repo, left) == tree_id(repo, right)
