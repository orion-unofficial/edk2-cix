#!/usr/bin/env python3
"""Helpers for replaying source-tree deltas onto a newer EDK2 base."""

from __future__ import annotations

import posixpath
import subprocess
import sys
from pathlib import Path
from typing import Iterable

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
from source_lifecycle import (
    SourceLifecycle,
    normalise_overlay_lifecycle,
    overlay_entries,
    project_overlay_tree,
    write_mirror_symlink,
)
from source_policy import NORMAL_FILE_MODES, overlay_source_path
from source_normalisation import (
    NormalisationResult,
    attribute_inconsistent_paths,
    modified_tracked_paths,
    normalise_worktree,
)


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


def unchanged_ours_conflicts(merge_output: str) -> set[str]:
    """Find false rename conflicts whose base and ours entries are identical."""

    entries: dict[str, dict[str, tuple[str, str]]] = {}
    for line in merge_output.splitlines():
        if "\t" not in line:
            continue
        meta, path = line.split("\t", 1)
        parts = meta.split()
        if (
            len(parts) == 3
            and parts[0] in {"100644", "100755", "120000"}
            and parts[2] in {"1", "2", "3"}
        ):
            entries.setdefault(path, {})[parts[2]] = (parts[0], parts[1])
    return {
        path
        for path, stages in entries.items()
        if set(stages) == {"1", "2"} and stages["1"] == stages["2"]
    }


def preserve_conflict_worktree(
    repo: Path,
    *,
    tree: str,
    label: str,
    merge_output: str,
    source_ref: str,
    new_base_ref: str,
    resume_variable: str,
    conflict_paths: Iterable[str] | None = None,
    conflict_stage: str = "source",
    verbose: bool,
) -> Path:
    scratch = temp_path(repo, f"port-{safe_name(label)}-conflict-")
    worktree = scratch / "worktree"
    paths = sorted(
        conflicted_paths(merge_output)
        if conflict_paths is None
        else set(conflict_paths)
    )
    conflict_commit = commit_tree(
        repo,
        tree,
        f"source-port: conflict tree for {label}\n\n"
        f"Source-Port-Input: {source_ref}\n"
        f"Source-Port-New-Base: {new_base_ref}\n"
        f"Source-Port-Conflict-Stage: {conflict_stage}\n",
    )
    git(repo, "worktree", "add", "--detach", str(worktree), conflict_commit, capture=not verbose)
    resume_lines = [
        f"    {resume_variable}=$(git -C {worktree} rev-parse HEAD)",
    ]
    if resume_variable == "REF":
        resume_lines.append("    MATERIALISE=0")
    notes = [
        f"# Source Port Conflict: {label}",
        "",
        "Git could not merge the old source tree onto the new base automatically.",
        "Resolve the conflict markers in the worktree, commit the result, then rerun the original integration command with:",
        "",
        *resume_lines,
        "",
        f"Conflict stage: {conflict_stage}",
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


def path_is_under(path: str, roots: Iterable[str]) -> bool:
    return any(path == root or path.startswith(root.rstrip("/") + "/") for root in roots)


def overlay_paths_from_source(
    repo: Path,
    *,
    tree: str,
    source_ref: str,
    paths: Iterable[str],
    label: str,
    verbose: bool,
) -> str:
    """Replace policy-owned paths in tree with their exact source_ref state."""

    selected = tuple(dict.fromkeys(path.strip("/") for path in paths if path.strip("/")))
    if not selected:
        return tree
    source = resolve_ref(repo, source_ref)
    overlay_commit = commit_tree(
        repo,
        tree,
        f"source-port: prepare policy overlay for {label}\n",
    )
    with temp_dir(repo, f"overlay-{safe_name(label)}-") as tmp:
        worktree = Path(tmp) / "worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), overlay_commit, capture=not verbose)
        try:
            for path in selected:
                git(
                    worktree,
                    "rm",
                    "-r",
                    "-f",
                    "--ignore-unmatch",
                    "--",
                    path,
                    check=False,
                    capture=not verbose,
                )
                exists = git(repo, "cat-file", "-e", f"{source}:{path}", check=False)
                if exists.returncode == 0:
                    git(worktree, "checkout", source, "--", path, capture=not verbose)
            return git(worktree, "write-tree").stdout.strip()
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False, capture=True)


def normalise_overlay_tree(
    repo: Path,
    *,
    tree: str,
    source_ref: str,
    label: str,
    verbose: bool,
) -> tuple[str, set[str], str]:
    """Apply source-path lifecycle and content rebases to a merged tree."""

    source = resolve_ref(repo, source_ref)
    paths = sorted(overlay_entries(repo, source))
    if not paths:
        return tree, set(), ""

    candidate = commit_tree(
        repo,
        tree,
        f"source-port: prepare lifecycle normalisation for {label}\n",
    )
    with temp_dir(repo, f"lifecycle-{safe_name(label)}-") as tmp:
        worktree = Path(tmp) / "worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), candidate, capture=not verbose)
        try:
            projections = project_overlay_tree(repo, source, candidate, paths)
            normalise_overlay_lifecycle(
                worktree,
                source_repo=repo,
                from_ref=source,
                to_ref=candidate,
                paths=paths,
                mode="exact",
                verbose=verbose,
            )
            lifecycle = SourceLifecycle(repo, source, candidate)
            source_overlays = overlay_entries(repo, source)
            conflicts: set[str] = set()
            details: list[str] = []
            blobs = Path(tmp) / "blobs"
            blobs.mkdir(parents=True, exist_ok=True)

            for index, projection in enumerate(projections):
                entry = source_overlays.get(projection.overlay_path)
                if (
                    entry is None
                    or entry.mode not in NORMAL_FILE_MODES
                    or projection.action not in {"keep", "rename"}
                    or not projection.source_path
                    or not projection.target_overlay_path
                ):
                    continue

                mapping = lifecycle.map_source_path(projection.source_path)
                if not mapping.target_path:
                    continue
                previous_source = lifecycle.from_entries.get(projection.source_path)
                new_source = lifecycle.to_entries.get(mapping.target_path)
                if (
                    previous_source is None
                    or new_source is None
                    or previous_source.mode not in NORMAL_FILE_MODES
                    or new_source.mode not in NORMAL_FILE_MODES
                ):
                    continue

                overlay_blob = git_blob_bytes(repo, entry.object_id)
                new_source_blob = git_blob_bytes(repo, new_source.object_id)
                if previous_source.object_id == new_source.object_id:
                    current = git(
                        worktree,
                        "ls-files",
                        "-s",
                        "--",
                        projection.target_overlay_path,
                    ).stdout.strip()
                    expected = f"{entry.mode} {entry.object_id} 0\t{projection.target_overlay_path}"
                    if current != expected:
                        conflicts.add(projection.target_overlay_path)
                        details.append(
                            "CONFLICT (overlay state): "
                            f"{projection.target_overlay_path} changed even though "
                            f"{projection.source_path} did not; review whether the "
                            "overlay was intentionally absorbed by related source changes"
                        )
                    continue
                if overlay_blob == new_source_blob:
                    write_mirror_symlink(worktree, projection.target_overlay_path)
                    continue
                previous_source_blob = git_blob_bytes(repo, previous_source.object_id)
                overlay_normalised = normalise_merge_text(overlay_blob)
                previous_normalised = normalise_merge_text(previous_source_blob)
                new_normalised = normalise_merge_text(new_source_blob)
                if (
                    overlay_normalised is None
                    or previous_normalised is None
                    or new_normalised is None
                ):
                    conflicts.add(projection.target_overlay_path)
                    details.append(
                        "CONFLICT (overlay content): "
                        f"{projection.target_overlay_path} contains binary data and requires review"
                    )
                    continue
                if overlay_normalised == new_normalised:
                    write_mirror_symlink(worktree, projection.target_overlay_path)
                    continue

                previous_path = blobs / f"{index}.base"
                overlay_path = blobs / f"{index}.overlay"
                new_path = blobs / f"{index}.new"
                previous_path.write_bytes(previous_normalised)
                overlay_path.write_bytes(overlay_normalised)
                new_path.write_bytes(new_normalised)
                merge = subprocess.run(
                    [
                        "git",
                        "merge-file",
                        "-p",
                        "-L",
                        f"{projection.overlay_path} (unofficial overlay)",
                        "-L",
                        f"{projection.source_path} (previous source)",
                        "-L",
                        f"{mapping.target_path} (new source)",
                        str(overlay_path),
                        str(previous_path),
                        str(new_path),
                    ],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=False,
                )
                target_path = worktree / projection.target_overlay_path
                if 0 <= merge.returncode <= 127:
                    if merge.stdout == new_normalised:
                        write_mirror_symlink(worktree, projection.target_overlay_path)
                        continue
                    target_path.write_bytes(merge.stdout)
                    target_path.chmod(0o755 if entry.mode == "100755" else 0o644)
                    git(worktree, "add", "--", projection.target_overlay_path)
                    if merge.returncode > 0:
                        conflicts.add(projection.target_overlay_path)
                        details.append(
                            "CONFLICT (overlay content): "
                            f"{projection.target_overlay_path} requires review while rebasing "
                            f"{projection.source_path} onto {mapping.target_path}"
                        )
                    continue

                conflicts.add(projection.target_overlay_path)
                detail = merge.stderr.decode("utf-8", errors="replace").strip()
                details.append(
                    "CONFLICT (overlay content): "
                    f"{projection.target_overlay_path} could not be merged automatically"
                    + (f": {detail}" if detail else "")
                )

            mirror_complete_overlay_additions(
                worktree,
                lifecycle=lifecycle,
                source_overlays=source_overlays,
                verbose=verbose,
            )
            return (
                git(worktree, "write-tree").stdout.strip(),
                conflicts,
                "\n".join(details),
            )
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False, capture=True)


def git_blob_bytes(repo: Path, object_id: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(repo), "cat-file", "-p", object_id],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise ReconstructionError(f"cannot read Git blob {object_id}: {detail}")
    return result.stdout


def mirror_complete_overlay_additions(
    worktree: Path,
    *,
    lifecycle: SourceLifecycle,
    source_overlays: dict[str, object],
    verbose: bool,
) -> None:
    overlay_names_by_dir: dict[str, set[str]] = {}
    for overlay_path in source_overlays:
        overlay_dir = posixpath.dirname(overlay_path)
        overlay_names_by_dir.setdefault(overlay_dir, set()).add(
            posixpath.basename(overlay_path)
        )

    old_source_names_by_dir: dict[str, set[str]] = {}
    for source_path in lifecycle.from_entries:
        source_dir = posixpath.dirname(source_path)
        old_source_names_by_dir.setdefault(source_dir, set()).add(
            posixpath.basename(source_path)
        )

    new_source_paths_by_dir: dict[str, list[str]] = {}
    for source_path in lifecycle.to_entries:
        new_source_paths_by_dir.setdefault(posixpath.dirname(source_path), []).append(
            source_path
        )

    for overlay_dir, overlay_names in sorted(overlay_names_by_dir.items()):
        representative = f"{overlay_dir}/placeholder"
        source_dir = posixpath.dirname(overlay_source_path(representative))
        old_source_names = old_source_names_by_dir.get(source_dir, set())
        if not old_source_names or not old_source_names.issubset(overlay_names):
            continue
        for source_path in sorted(new_source_paths_by_dir.get(source_dir, [])):
            overlay_path = "custom/overlay/" + source_path[len("src/") :]
            filesystem_path = worktree / overlay_path
            if filesystem_path.exists() or filesystem_path.is_symlink():
                continue
            if verbose:
                print(
                    f"[source-lifecycle] mirror new complete-overlay file: {overlay_path}",
                    file=sys.stderr,
                )
            write_mirror_symlink(worktree, overlay_path)


def normalise_merge_text(data: bytes) -> bytes | None:
    if b"\0" in data:
        return None
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def normalise_source_tree(
    repo: Path,
    *,
    tree: str,
    label: str,
    verbose: bool,
    paths: Iterable[str] = (),
) -> tuple[str, NormalisationResult]:
    """Canonicalise selected editable-source paths without changing raw vendor refs."""

    candidate = commit_tree(
        repo,
        tree,
        f"source-port: prepare source normalisation for {label}\n",
    )
    with temp_dir(repo, f"normalise-{safe_name(label)}-") as tmp:
        worktree = Path(tmp) / "worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), candidate, capture=not verbose)
        try:
            selected_paths = list(
                dict.fromkeys(
                    [
                        *paths,
                        *modified_tracked_paths(worktree),
                        *attribute_inconsistent_paths(worktree),
                    ]
                )
            )
            result = normalise_worktree(worktree, paths=selected_paths)
            if verbose and result.changed:
                print(
                    "Normalised source tree: "
                    f"{result.line_endings} line-ending file(s), "
                    f"{result.trailing_whitespace} trailing-whitespace file(s), "
                    f"{result.file_modes} file mode(s)",
                    file=sys.stderr,
                )
            if not result.changed:
                return tree, result
            return git(worktree, "write-tree").stdout.strip(), result
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False, capture=True)


def merge_source_tree(
    repo: Path,
    old_base: str,
    new_base: str,
    source_ref: str,
    *,
    label: str,
    new_base_ref: str,
    source_owned_paths: Iterable[str],
    resume_variable: str,
    verbose: bool,
) -> str:
    result = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "-c",
            "merge.renames=false",
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
    conflicts = conflicted_paths(result.stdout)
    if result.returncode != 0:
        tree = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
        if not tree or git(repo, "cat-file", "-t", tree, check=False).stdout.strip() != "tree":
            detail = compact_apply_detail((result.stderr or result.stdout or "unknown merge failure").strip())
            raise ReconstructionError(
                f"could not three-way merge source delta from {source_ref} onto {new_base}: {detail}"
            )
        policy_conflicts = {
            path for path in conflicts if path_is_under(path, source_owned_paths)
        }
        if policy_conflicts:
            tree = overlay_paths_from_source(
                repo,
                tree=tree,
                source_ref=source_ref,
                paths=source_owned_paths,
                label=label,
                verbose=verbose,
            )
            if verbose:
                print(
                    f"Resolved {len(policy_conflicts)} policy-owned conflict(s) from {source_ref}",
                    file=sys.stderr,
                )
            conflicts -= policy_conflicts
        unchanged_ours = unchanged_ours_conflicts(result.stdout)
        if unchanged_ours:
            conflicts -= unchanged_ours
            if verbose:
                print(
                    "Ignored "
                    f"{len(unchanged_ours)} false rename conflict(s) whose "
                    "base and new-base entries are identical",
                    file=sys.stderr,
                )
        if conflicts:
            worktree = preserve_conflict_worktree(
                repo,
                tree=tree,
                label=label,
                merge_output=result.stdout,
                source_ref=source_ref,
                new_base_ref=new_base_ref,
                resume_variable=resume_variable,
                conflict_paths=conflicts,
                conflict_stage="source",
                verbose=verbose,
            )
            suffix = " MATERIALISE=0" if resume_variable == "REF" else ""
            raise ReconstructionError(
                f"could not three-way merge source delta from {source_ref} onto {new_base}"
                f"\n\nsource-port conflict worktree preserved at: {worktree}\n"
                "Resolve conflicts there, commit the result, and rerun with "
                f"{resume_variable}=<resolved-commit>{suffix}."
            )
    else:
        tree = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
    if not tree:
        raise ReconstructionError(f"git merge-tree produced no tree for source delta from {source_ref}")
    kind = git(repo, "cat-file", "-t", tree).stdout.strip()
    if kind != "tree":
        raise ReconstructionError(f"git merge-tree produced a {kind}, not a tree, for source delta from {source_ref}")
    tree = overlay_paths_from_source(
        repo,
        tree=tree,
        source_ref=source_ref,
        paths=source_owned_paths,
        label=label,
        verbose=verbose,
    )
    tree, overlay_conflicts, overlay_detail = normalise_overlay_tree(
        repo,
        tree=tree,
        source_ref=source_ref,
        label=label,
        verbose=verbose,
    )
    if overlay_conflicts:
        worktree = preserve_conflict_worktree(
            repo,
            tree=tree,
            label=label,
            merge_output=overlay_detail,
            source_ref=source_ref,
            new_base_ref=new_base_ref,
            resume_variable=resume_variable,
            conflict_paths=overlay_conflicts,
            conflict_stage="overlay",
            verbose=verbose,
        )
        suffix = " MATERIALISE=0" if resume_variable == "REF" else ""
        raise ReconstructionError(
            "could not rebase custom overlays onto the new source tree"
            f"\n\nsource-port conflict worktree preserved at: {worktree}\n"
            "Resolve conflicts there, commit the result, and rerun with "
            f"{resume_variable}=<resolved-commit>{suffix}."
        )
    return tree


def apply_source_delta_to_base(
    repo: Path,
    *,
    old_base_ref: str,
    source_ref: str,
    new_base_ref: str,
    message: str,
    label: str,
    source_owned_paths: Iterable[str] = (),
    normalise_source: bool = False,
    resume_variable: str = "REF",
    verbose: bool,
) -> str:
    """Replay the source_ref delta from old_base_ref onto new_base_ref."""

    old_base = materialised_base_commit(repo, old_base_ref, label=f"{label}-old-base", verbose=verbose)
    new_base = materialised_base_commit(repo, new_base_ref, label=f"{label}-new-base", verbose=verbose)
    source_delta_paths = [
        line
        for line in git(repo, "diff", "--name-only", old_base, source_ref).stdout.splitlines()
        if line
    ]
    tree = merge_source_tree(
        repo,
        old_base,
        new_base,
        source_ref,
        label=label,
        new_base_ref=new_base_ref,
        source_owned_paths=source_owned_paths,
        resume_variable=resume_variable,
        verbose=verbose,
    )
    if normalise_source:
        tree, _result = normalise_source_tree(
            repo,
            tree=tree,
            label=label,
            verbose=verbose,
            paths=source_delta_paths,
        )
    if tree == tree_id(repo, new_base):
        return new_base
    return commit_tree(repo, tree, message)


def same_tree(repo: Path, left: str, right: str) -> bool:
    return tree_id(repo, left) == tree_id(repo, right)
