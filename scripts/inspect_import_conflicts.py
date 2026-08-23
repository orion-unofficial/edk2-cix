#!/usr/bin/env python3
"""Inspect paused import conflicts with symlink-aware context."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import posixpath
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from import_workflow import operations_root
from reconstruction_common import ReconstructionError, git, main_wrapper, repo_root, safe_name


STAGE_LABELS = {
    1: "base",
    2: "ours (target branch)",
    3: "theirs (incoming change)",
}
SYMLINK_MODE = "120000"
REGULAR_MODES = {"100644", "100755"}
OP_NAMESPACES = ("import-changes", "import-unofficial")


@dataclass(frozen=True)
class StageEntry:
    mode: str
    object_id: str
    stage: int
    path: str


def parse_unmerged_line(line: str) -> StageEntry:
    metadata, path = line.split("\t", 1)
    mode, object_id, stage = metadata.split()
    return StageEntry(mode=mode, object_id=object_id, stage=int(stage), path=path)


def unmerged_entries(repo: Path) -> dict[str, dict[int, StageEntry]]:
    result = git(repo, "ls-files", "-u", check=False)
    entries: dict[str, dict[int, StageEntry]] = {}
    for line in result.stdout.splitlines():
        if not line:
            continue
        entry = parse_unmerged_line(line)
        entries.setdefault(entry.path, {})[entry.stage] = entry
    return entries


def logical_conflicts(entries: dict[str, dict[int, StageEntry]]) -> dict[str, dict[int, StageEntry]]:
    """Combine Git's distinct-type side files back into one logical conflict."""

    combined: dict[str, dict[int, StageEntry]] = {}
    consumed: set[str] = set()
    for path, stages in sorted(entries.items()):
        if path in consumed:
            continue
        logical_path = path
        if "~" in path:
            candidate = path.rsplit("~", 1)[0]
            if candidate in entries:
                logical_path = candidate
        merged = dict(entries.get(logical_path, {}))
        for suffix_path, suffix_stages in entries.items():
            if suffix_path.startswith(f"{logical_path}~"):
                merged.update(suffix_stages)
                consumed.add(suffix_path)
        consumed.add(logical_path)
        combined[logical_path] = merged
    return combined


def cat_blob(repo: Path, object_id: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(repo), "cat-file", "-p", object_id],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        return b""
    return result.stdout


def show_index_path(repo: Path, path: str, stage: int | None = None) -> bytes | None:
    spec = f":{stage}:{path}" if stage is not None else f":{path}"
    result = subprocess.run(
        ["git", "-C", str(repo), "show", spec],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode == 0:
        return result.stdout
    return None


def blob_bytes(repo: Path, entry: StageEntry) -> bytes:
    return cat_blob(repo, entry.object_id)


def mode_name(mode: str) -> str:
    if mode == SYMLINK_MODE:
        return "symlink"
    if mode == "100755":
        return "executable file"
    if mode == "100644":
        return "regular file"
    return f"mode {mode}"


def digest(data: bytes | None) -> str:
    if data is None:
        return "unavailable"
    return f"sha256:{hashlib.sha256(data).hexdigest()[:16]}, {len(data)} byte(s)"


def symlink_target(repo: Path, entry: StageEntry) -> str:
    return blob_bytes(repo, entry).decode("utf-8", errors="replace")


def resolve_link(path: str, target: str) -> str:
    if posixpath.isabs(target):
        return target
    return posixpath.normpath(posixpath.join(posixpath.dirname(path), target))


def symlink_target_content(repo: Path, path: str, entry: StageEntry) -> tuple[str, bytes | None]:
    target = symlink_target(repo, entry)
    resolved = resolve_link(path, target)
    # Prefer the same conflict stage, then the resolved index, then HEAD. Most
    # overlay symlinks point at an already-resolved src/ path, so the stage-0
    # index lookup is usually the useful one.
    for stage in (entry.stage, None):
        data = show_index_path(repo, resolved, stage=stage)
        if data is not None:
            return resolved, data
    head = subprocess.run(
        ["git", "-C", str(repo), "show", f"HEAD:{resolved}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if head.returncode == 0:
        return resolved, head.stdout
    return resolved, None


def write_bytes(path: Path, data: bytes | None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if data is None:
        path.write_text("<unavailable>\n", encoding="utf-8")
    else:
        path.write_bytes(data)


def write_expanded_files(repo: Path, out_dir: Path, path: str, stages: dict[int, StageEntry]) -> list[str]:
    written: list[str] = []
    path_dir = out_dir / safe_name(path)
    if path_dir.exists():
        shutil.rmtree(path_dir)
    path_dir.mkdir(parents=True, exist_ok=True)
    for stage, entry in sorted(stages.items()):
        label = STAGE_LABELS.get(stage, f"stage-{stage}").split()[0]
        data = blob_bytes(repo, entry)
        if entry.mode == SYMLINK_MODE:
            write_bytes(path_dir / f"{label}.symlink", data)
            resolved, target_data = symlink_target_content(repo, path, entry)
            write_bytes(path_dir / f"{label}.symlink-target", target_data)
            (path_dir / f"{label}.symlink-target.path").write_text(f"{resolved}\n", encoding="utf-8")
            written.extend(
                [
                    str(path_dir / f"{label}.symlink"),
                    str(path_dir / f"{label}.symlink-target"),
                    str(path_dir / f"{label}.symlink-target.path"),
                ]
            )
        else:
            write_bytes(path_dir / label, data)
            written.append(str(path_dir / label))
    return written


def describe_path(repo: Path, path: str, stages: dict[int, StageEntry], expanded_root: Path | None) -> list[str]:
    modes = {entry.mode for entry in stages.values()}
    lines = [f"- {path}"]
    if SYMLINK_MODE in modes and modes & REGULAR_MODES:
        lines.append("  conflict type: symlink/file conflict")
    else:
        lines.append("  conflict type: content or mode conflict")

    regular_content: dict[int, bytes] = {
        stage: blob_bytes(repo, entry)
        for stage, entry in stages.items()
        if entry.mode in REGULAR_MODES
    }
    for stage, entry in sorted(stages.items()):
        label = STAGE_LABELS.get(stage, f"stage {stage}")
        if entry.mode == SYMLINK_MODE:
            target = symlink_target(repo, entry)
            resolved, target_data = symlink_target_content(repo, path, entry)
            lines.append(f"  {label}: symlink -> {target}")
            if entry.path != path:
                lines.append(f"    recorded as: {entry.path}")
            lines.append(f"    resolves to: {resolved}")
            lines.append(f"    expanded target content: {digest(target_data)}")
            for other_stage, other_content in sorted(regular_content.items()):
                other_label = STAGE_LABELS.get(other_stage, f"stage {other_stage}")
                if target_data is None:
                    relation = "unavailable"
                else:
                    relation = "identical" if target_data == other_content else "different"
                lines.append(f"    vs {other_label} regular file: {relation}")
        else:
            lines.append(f"  {label}: {mode_name(entry.mode)} ({digest(blob_bytes(repo, entry))})")
            if entry.path != path:
                lines.append(f"    recorded as: {entry.path}")

    if expanded_root is not None:
        written = write_expanded_files(repo, expanded_root, path, stages)
        lines.append("  expanded files:")
        lines.extend(f"    {item}" for item in written)
    return lines


def render_conflict_report(repo: Path, *, paths: list[str] | None = None, expanded_root: Path | None = None) -> str:
    entries = unmerged_entries(repo)
    entries = logical_conflicts(entries)
    if paths:
        wanted = set(paths)
        entries = {path: stages for path, stages in entries.items() if path in wanted}
    lines = [f"Conflict report for {repo}"]
    lines.append("Stage labels: base = common ancestor, ours = target branch, theirs = incoming change.")
    if not entries:
        lines.append("No unmerged index entries were found in this scratch tree.")
        return "\n".join(lines) + "\n"
    for path, stages in sorted(entries.items()):
        lines.extend(describe_path(repo, path, stages, expanded_root))
    return "\n".join(lines) + "\n"


def write_conflict_report(repo: Path, report_path: Path, *, paths: list[str] | None = None) -> str:
    expanded_root = report_path.parent / f"{report_path.stem}-expanded"
    report = render_conflict_report(repo, paths=paths, expanded_root=expanded_root)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(report, encoding="utf-8")
    return report


def operation_dir(repo: Path, namespace: str, op_id: str) -> Path:
    path = operations_root(repo, namespace) / op_id
    if not path.exists():
        raise ReconstructionError(f"operation does not exist: {namespace}/{op_id}")
    return path


def find_operation(repo: Path, op_id: str, namespace: str) -> tuple[str, Path]:
    namespaces = [namespace] if namespace else list(OP_NAMESPACES)
    matches: list[tuple[str, Path]] = []
    for item in namespaces:
        root = operations_root(repo, item)
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


def inspect_operation(repo: Path, namespace: str, op_dir: Path) -> None:
    state_path = op_dir / "state.json"
    if not state_path.exists():
        raise ReconstructionError(f"operation has no state.json: {op_dir}")
    state = json.loads(state_path.read_text(encoding="utf-8"))
    reports = op_dir / "reports"
    targets = [target for target in state.get("targets", []) if target.get("status") == "conflict"]
    if not targets:
        print(f"No conflicted targets recorded in {namespace}/{op_dir.name}")
        return
    for target in targets:
        scratch = Path(target["scratch"])
        report_path = reports / f"{safe_name(target['ref'])}.txt"
        report = write_conflict_report(scratch, report_path, paths=target.get("conflict_paths") or None)
        print(report.rstrip())
        print(f"Report written to: {report_path}")


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--scratch", default=os.environ.get("SCRATCH", ""), help="inspect one scratch tree directly")
    p.add_argument("--op-id", default=os.environ.get("OP_ID", ""), help="paused import operation id")
    p.add_argument(
        "--import-tool",
        default=os.environ.get("IMPORT_TOOL", ""),
        choices=OP_NAMESPACES,
        help="operation namespace when OP_ID is ambiguous",
    )
    p.add_argument("--report", default=os.environ.get("REPORT", ""), help="optional report path for SCRATCH mode")
    return p


def main() -> None:
    args = parser().parse_args()
    if args.scratch:
        scratch = Path(args.scratch)
        report_path = Path(args.report) if args.report else scratch / ".git" / "edk2-cix-conflict-report.txt"
        report = write_conflict_report(scratch, report_path)
        print(report.rstrip())
        print(f"Report written to: {report_path}")
        return
    repo = repo_root(Path(__file__))
    namespace, op_dir = find_operation(repo, args.op_id, args.import_tool)
    inspect_operation(repo, namespace, op_dir)


if __name__ == "__main__":
    main_wrapper(main)
