#!/usr/bin/env python3
"""Resolve paused import conflicts with symlink-aware vimdiff panes."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import stat
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from import_workflow import load_state
from inspect_import_conflicts import (
    OP_NAMESPACES,
    STAGE_LABELS,
    SYMLINK_MODE,
    StageEntry,
    blob_bytes,
    logical_conflicts,
    symlink_target,
    symlink_target_content,
    unmerged_entries,
    write_conflict_report,
)
from reconstruction_common import ReconstructionError, cache_dir, git, main_wrapper, repo_root, safe_name, truthy


HELP = """resolve-import-conflicts

Batch-resolve conflicts from a paused import operation.

Examples:
  make resolve-conflicts OP_ID=<operation-id>
  make resolve-conflicts OP_ID=<operation-id> IMPORT_TOOL=import-unofficial
  make resolve-conflicts SCRATCH=<scratch-tree> CONFLICT_PATHS=path/to/file.c

Environment:
  OP_ID=<id>                         Paused import operation id.
  IMPORT_TOOL=import-changes|import-unofficial
                                     Operation namespace when OP_ID is ambiguous.
  SCRATCH=<path>                     Resolve one scratch tree directly.
  CONFLICT_PATHS=<path[,path...]>    Optional comma-separated logical conflict paths.
  CONFLICT_EDITOR=<command>          Editor command. Default: vimdiff -f.
  PRESERVE_SYMLINKS=0|1              If a resolved file exactly matches an expanded
                                     conflicted symlink target, restore that symlink
                                     instead of materialising a regular file.
                                     Default: 1.
  ALLOW_CONFLICT_MARKERS=0|1         Permit unresolved conflict-marker text.
                                     Default: 0.

This helper edits and stages only scratch trees under .cache. It never moves
source refs or tags. After resolving, run the relevant import CONTINUE command
without WRITE=1 to validate candidates, then add WRITE=1 only when ready to
persist the already-validated candidates.
"""

MARKERS = (b"<<<<<<< ", b"=======", b">>>>>>> ")


@dataclass(frozen=True)
class ConflictTarget:
    ref: str
    scratch: Path
    paths: list[str]
    state_target: dict[str, object] | None = None


@dataclass(frozen=True)
class Variant:
    label: str
    entry: StageEntry
    data: bytes
    description: str
    symlink_target: str | None = None


def progress(message: str) -> None:
    print(f"[resolve] {message}", file=sys.stderr, flush=True)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, epilog=HELP, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--scratch", default=os.environ.get("SCRATCH", ""), help="resolve one scratch tree directly")
    p.add_argument("--op-id", default=os.environ.get("OP_ID", ""), help="paused import operation id")
    p.add_argument(
        "--import-tool",
        default=os.environ.get("IMPORT_TOOL", ""),
        choices=OP_NAMESPACES,
        help="operation namespace when OP_ID is ambiguous",
    )
    p.add_argument(
        "--conflict-paths",
        default=os.environ.get("CONFLICT_PATHS", ""),
        help="optional comma-separated logical conflict paths",
    )
    p.add_argument("--editor", default=os.environ.get("CONFLICT_EDITOR", ""), help="editor command; default: vimdiff -f")
    p.add_argument("--preserve-symlinks", default=os.environ.get("PRESERVE_SYMLINKS", "1"))
    p.add_argument("--allow-conflict-markers", default=os.environ.get("ALLOW_CONFLICT_MARKERS", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def split_paths(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def find_operation(repo: Path, op_id: str, namespace: str) -> tuple[str, Path]:
    namespaces = [namespace] if namespace else list(OP_NAMESPACES)
    matches: list[tuple[str, Path]] = []
    for item in namespaces:
        root = cache_dir(repo, "operations", item)
        if not root.exists():
            continue
        if op_id:
            candidate = root / op_id
            if candidate.exists():
                matches.append((item, candidate))
        else:
            matches.extend((item, path) for path in root.iterdir() if path.is_dir())
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise ReconstructionError("no paused import operation found; pass OP_ID=<id> or SCRATCH=<path>")
    lines = ["multiple paused import operations found; pass OP_ID=<id> and, if needed, IMPORT_TOOL=<tool>:"]
    lines.extend(f"  - {name}/{path.name}" for name, path in sorted(matches))
    raise ReconstructionError("\n".join(lines))


def operation_targets(op_dir: Path, paths: list[str]) -> list[ConflictTarget]:
    state = load_state(op_dir)
    targets: list[ConflictTarget] = []
    for target in state.get("targets", []):
        if target.get("status") != "conflict":
            continue
        scratch = Path(str(target["scratch"]))
        target_paths = paths or [str(item) for item in target.get("conflict_paths", [])]
        targets.append(ConflictTarget(ref=str(target["ref"]), scratch=scratch, paths=target_paths, state_target=target))
    if not targets:
        raise ReconstructionError(f"operation has no conflicted targets: {op_dir}")
    return targets


def direct_target(scratch: Path, paths: list[str]) -> list[ConflictTarget]:
    return [ConflictTarget(ref=scratch.name, scratch=scratch, paths=paths)]


def conflict_entries(repo: Path, paths: list[str]) -> dict[str, dict[int, StageEntry]]:
    entries = logical_conflicts(unmerged_entries(repo))
    if paths:
        requested = set(paths)
        entries = {path: stages for path, stages in entries.items() if path in requested}
        missing = sorted(requested - set(entries))
        if missing:
            raise ReconstructionError("requested path(s) are not unmerged conflicts:\n" + "\n".join(f"  - {path}" for path in missing))
    return entries


def variant_for_entry(repo: Path, logical_path: str, stage: int, entry: StageEntry) -> Variant:
    label = STAGE_LABELS.get(stage, f"stage {stage}")
    if entry.mode == SYMLINK_MODE:
        target = symlink_target(repo, entry)
        resolved, target_data = symlink_target_content(repo, logical_path, entry)
        data = target_data if target_data is not None else blob_bytes(repo, entry)
        description = f"{label}: symlink -> {target}; expanded from {resolved}"
        return Variant(label=label, entry=entry, data=data, description=description, symlink_target=target)
    data = blob_bytes(repo, entry)
    return Variant(label=label, entry=entry, data=data, description=f"{label}: regular file")


def read_worktree_resolution(repo: Path, logical_path: str, variants: dict[int, Variant]) -> bytes:
    path = repo / logical_path
    if path.is_symlink():
        target = os.readlink(path)
        for stage in (2, 3, 1):
            variant = variants.get(stage)
            if variant and variant.symlink_target == target:
                return variant.data
    if path.is_file():
        return path.read_bytes()
    for stage in (2, 3, 1):
        variant = variants.get(stage)
        if variant:
            return variant.data
    return b""


def write_variant_file(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def prepare_resolution_files(repo: Path, resolution_dir: Path, logical_path: str, stages: dict[int, StageEntry]) -> tuple[Path, list[Path], dict[int, Variant]]:
    if resolution_dir.exists():
        shutil.rmtree(resolution_dir)
    resolution_dir.mkdir(parents=True, exist_ok=True)
    variants = {stage: variant_for_entry(repo, logical_path, stage, entry) for stage, entry in sorted(stages.items())}
    pane_paths: list[Path] = []
    for stage in (1, 2, 3):
        variant = variants.get(stage)
        if not variant:
            continue
        name = {
            1: "base.common-ancestor",
            2: "ours.target-branch",
            3: "theirs.incoming-change",
        }.get(stage, STAGE_LABELS.get(stage, f"stage-{stage}").split()[0])
        path = resolution_dir / f"{name}.txt"
        write_variant_file(path, variant.data)
        if stage in (2, 3):
            pane_paths.append(path)
    resolved = resolution_dir / "resolved.txt"
    initial = read_worktree_resolution(repo, logical_path, variants)
    write_variant_file(resolved, initial)
    metadata = {
        "logical_path": logical_path,
        "panes": [str(path) for path in pane_paths],
        "resolved": str(resolved),
        "variants": {str(stage): variant.description for stage, variant in variants.items()},
    }
    (resolution_dir / "metadata.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return resolved, [*pane_paths, resolved], variants


def editor_command(args: argparse.Namespace) -> list[str]:
    if args.editor:
        return shlex.split(args.editor)
    vimdiff = shutil.which("vimdiff")
    if not vimdiff:
        raise ReconstructionError("vimdiff is not available; set CONFLICT_EDITOR=<command>")
    if not sys.stdin.isatty():
        raise ReconstructionError("resolve-conflicts needs an interactive terminal; set CONFLICT_EDITOR=<command> for non-interactive use")
    return [vimdiff, "-f"]


def run_editor(command: list[str], files: list[Path]) -> None:
    result = subprocess.run([*command, *(str(path) for path in files)], check=False)
    if result.returncode != 0:
        raise ReconstructionError(f"conflict editor failed with exit status {result.returncode}: {' '.join(command)}")


def has_conflict_markers(data: bytes) -> bool:
    return all(marker in data for marker in MARKERS)


def matching_symlink_variant(resolved: bytes, variants: dict[int, Variant]) -> Variant | None:
    for stage in (2, 3, 1):
        variant = variants.get(stage)
        if variant and variant.entry.mode == SYMLINK_MODE and variant.data == resolved and variant.symlink_target:
            return variant
    return None


def write_resolution_to_worktree(
    repo: Path,
    logical_path: str,
    stages: dict[int, StageEntry],
    variants: dict[int, Variant],
    resolved: bytes,
    *,
    preserve_symlinks: bool,
) -> str:
    output = repo / logical_path
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() or output.is_symlink():
        if output.is_dir() and not output.is_symlink():
            shutil.rmtree(output)
        else:
            output.unlink()

    written_as = "regular file"
    symlink_variant = matching_symlink_variant(resolved, variants) if preserve_symlinks else None
    if symlink_variant is not None:
        os.symlink(symlink_variant.symlink_target, output)
        written_as = f"symlink -> {symlink_variant.symlink_target}"
    else:
        output.write_bytes(resolved)
        executable = any(entry.mode == "100755" and blob_bytes(repo, entry) == resolved for entry in stages.values())
        if executable:
            output.chmod(output.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    for entry in stages.values():
        if entry.path == logical_path:
            continue
        side = repo / entry.path
        if side.exists() or side.is_symlink():
            if side.is_dir() and not side.is_symlink():
                shutil.rmtree(side)
            else:
                side.unlink()
        git(repo, "rm", "--cached", "--quiet", "--ignore-unmatch", "--", entry.path, check=False)
    git(repo, "add", "--", logical_path)
    return written_as


def reject_files(repo: Path) -> list[str]:
    tracked = git(repo, "ls-files", "*.rej", check=False).stdout.splitlines()
    untracked = git(repo, "ls-files", "--others", "--exclude-standard", "*.rej", check=False).stdout.splitlines()
    return sorted(set(tracked + untracked))


def resolve_target(target: ConflictTarget, command: list[str], args: argparse.Namespace, op_dir: Path | None, target_index: int, target_count: int) -> int:
    repo = target.scratch
    if not repo.exists():
        raise ReconstructionError(f"scratch tree does not exist: {repo}")
    entries = conflict_entries(repo, target.paths)
    if not entries:
        progress(f"{target.ref}: no unmerged conflict entries")
        rejects = reject_files(repo)
        if rejects:
            raise ReconstructionError(
                f"{target.ref} has reject files but no index conflicts; resolve these manually:\n"
                + "\n".join(f"  - {path}" for path in rejects)
            )
        return 0

    report_root = op_dir / "reports" if op_dir else repo / ".git"
    report_path = report_root / f"{safe_name(target.ref)}.txt"
    write_conflict_report(repo, report_path, paths=list(entries))
    progress(f"target {target_index}/{target_count}: {target.ref}")
    progress(f"symlink-aware conflict report: {report_path}")

    resolved_count = 0
    for path_index, (logical_path, stages) in enumerate(sorted(entries.items()), start=1):
        progress(f"resolving {path_index}/{len(entries)}: {logical_path}")
        resolution_root = (op_dir / "resolutions" if op_dir else repo / ".git" / "edk2-cix-resolutions") / safe_name(target.ref)
        resolved_path, pane_paths, variants = prepare_resolution_files(
            repo,
            resolution_root / safe_name(logical_path),
            logical_path,
            stages,
        )
        progress("opening editor panes: " + ", ".join(path.name for path in pane_paths))
        run_editor(command, pane_paths)
        resolved = resolved_path.read_bytes()
        if has_conflict_markers(resolved) and not truthy(args.allow_conflict_markers):
            raise ReconstructionError(
                f"resolved file still appears to contain conflict markers: {resolved_path}\n"
                "Set ALLOW_CONFLICT_MARKERS=1 only if those marker strings are intentional."
            )
        written_as = write_resolution_to_worktree(
            repo,
            logical_path,
            stages,
            variants,
            resolved,
            preserve_symlinks=truthy(args.preserve_symlinks),
        )
        progress(f"staged {logical_path} as {written_as}")
        resolved_count += 1

    unresolved = git(repo, "diff", "--name-only", "--diff-filter=U", check=False).stdout.splitlines()
    if unresolved:
        raise ReconstructionError("scratch tree still has unresolved paths:\n" + "\n".join(f"  - {path}" for path in unresolved))
    rejects = reject_files(repo)
    if rejects:
        raise ReconstructionError("scratch tree still has reject files:\n" + "\n".join(f"  - {path}" for path in rejects))
    return resolved_count


def main() -> None:
    args = parser().parse_args()
    progress("Starting conflict resolver")
    command = editor_command(args)
    paths = split_paths(args.conflict_paths)
    if args.scratch:
        targets = direct_target(Path(args.scratch), paths)
        op_dir = None
        namespace = "scratch"
        op_id = Path(args.scratch).name
    else:
        repo = repo_root(Path(__file__))
        namespace, op_dir = find_operation(repo, args.op_id, args.import_tool)
        op_id = op_dir.name
        targets = operation_targets(op_dir, paths)

    progress(f"operation: {namespace}/{op_id}")
    progress(f"editor: {' '.join(command)}")
    total = 0
    for index, target in enumerate(targets, start=1):
        total += resolve_target(target, command, args, op_dir, index, len(targets))
    print(f"resolved and staged {total} conflict path(s) in scratch tree(s)")
    print("No source refs or tags were moved.")
    print("Next validate the candidate commits, for example:")
    if namespace == "import-unofficial":
        print(f"  make import-unofficial-commits CONTINUE=1 OP_ID={op_id}")
        print("Then persist only after validation:")
        print(f"  make import-unofficial-commits CONTINUE=1 OP_ID={op_id} WRITE=1")
    elif namespace == "import-changes":
        print(f"  make import-changes CONTINUE=1 OP_ID={op_id}")
        print("Then persist only after validation:")
        print(f"  make import-changes CONTINUE=1 OP_ID={op_id} WRITE=1")


if __name__ == "__main__":
    main_wrapper(main)
