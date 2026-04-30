#!/usr/bin/env python3
"""Shared helpers for the EDK2-CIX firmware source tooling."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable


class ReconstructionError(RuntimeError):
    """Raised for expected user-facing workflow failures."""


def run(cmd: list[str], cwd: Path | str | None = None, check: bool = True, capture: bool = True) -> subprocess.CompletedProcess[str]:
    kwargs: dict[str, Any] = {"text": True}
    if capture:
        kwargs.update({"stdout": subprocess.PIPE, "stderr": subprocess.PIPE})
    result = subprocess.run(cmd, cwd=str(cwd) if cwd else None, **kwargs)
    if check and result.returncode != 0:
        stderr = (result.stderr or "").strip()
        stdout = (result.stdout or "").strip()
        detail = stderr or stdout or f"exit status {result.returncode}"
        raise ReconstructionError(f"command failed: {' '.join(cmd)}\n{detail}")
    return result


def git(repo: Path, *args: str, check: bool = True, capture: bool = True) -> subprocess.CompletedProcess[str]:
    return run(["git", "-C", str(repo), *args], check=check, capture=capture)


def repo_root(start: Path | str | None = None) -> Path:
    base = Path(start) if start else Path(__file__).resolve().parent
    if base.is_file():
        base = base.parent
    result = run(["git", "-C", str(base), "rev-parse", "--show-toplevel"])
    return Path(result.stdout.strip())


def load_json(repo: Path, relative: str) -> dict[str, Any]:
    path = repo / relative
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def truthy(value: str | int | bool | None) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return False
    return str(value).strip().lower() in {"1", "true", "yes", "on"}


def release_to_branch(release: str) -> str:
    if release.startswith("refs/heads/"):
        release = release[len("refs/heads/") :]
    if release.startswith("source/release/"):
        return release
    return f"source/release/{release}"


def branch_to_ref(branch: str) -> str:
    return branch if branch.startswith("refs/") else f"refs/heads/{branch}"


def short_release(branch: str) -> str:
    return branch[len("source/release/") :] if branch.startswith("source/release/") else branch


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_") or "release"


def ref_exists(repo: Path, ref: str) -> bool:
    return git(repo, "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}", check=False).returncode == 0


def rev_parse(repo: Path, ref: str) -> str:
    return git(repo, "rev-parse", f"{ref}^{{commit}}").stdout.strip()


def tree_id(repo: Path, ref: str) -> str:
    return git(repo, "rev-parse", f"{ref}^{{tree}}").stdout.strip()


def ref_type(repo: Path, ref: str) -> str | None:
    result = git(repo, "cat-file", "-t", ref, check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def show_file(repo: Path, ref: str, path: str, check: bool = True) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(repo), "show", f"{ref}:{path}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        raise ReconstructionError(
            f"could not read {path} from {ref}: {result.stderr.decode('utf-8', errors='ignore').strip()}"
        )
    return result.stdout


def fetch_branch_from_origin(repo: Path, branch: str, verbose: bool = False) -> bool:
    refspec = f"refs/heads/{branch}:refs/remotes/origin/{branch}"
    result = git(repo, "fetch", "origin", refspec, check=False, capture=True)
    if result.returncode == 0:
        if verbose:
            print(f"Fetched {branch} from origin", file=sys.stderr)
        return True
    if verbose:
        warning = (result.stderr or result.stdout or "unknown fetch failure").strip()
        print(f"warning: could not fetch {branch} from origin: {warning}", file=sys.stderr)
    return False


def resolve_branch_or_origin(repo: Path, branch: str, verbose: bool = False) -> str | None:
    candidates = [branch, f"origin/{branch}"]
    for candidate in candidates:
        if ref_exists(repo, candidate):
            return candidate
    fetch_branch_from_origin(repo, branch, verbose=verbose)
    for candidate in candidates:
        if ref_exists(repo, candidate):
            return candidate
    return None


def release_entry(repo: Path, release: str | None, require: bool = False) -> tuple[str, dict[str, Any]]:
    releases = load_json(repo, "config/releases.json")
    selected = release or releases.get("default_release")
    if not selected:
        if require:
            raise ReconstructionError("RELEASE is required and no default release is configured")
        raise ReconstructionError("no release selected")
    branch = release_to_branch(selected)
    entries: dict[str, Any] = releases.get("releases", {})
    entry = entries.get(branch) or entries.get(short_release(branch))
    if entry is None:
        raise ReconstructionError(f"unknown release: {selected}\nUse make help-vars to list configured releases.")
    return branch, entry


def cache_dir(repo: Path, *parts: str) -> Path:
    path = repo / ".cache" / "edk2-cix"
    for part in parts:
        path /= part
    path.mkdir(parents=True, exist_ok=True)
    return path


def temp_dir(repo: Path, prefix: str) -> tempfile.TemporaryDirectory[str]:
    root = cache_dir(repo, "tmp")
    return tempfile.TemporaryDirectory(prefix=prefix, dir=root)


def commit_tree_with_files(repo: Path, files: dict[str, bytes], message: str, parents: list[str] | None = None) -> str:
    entries: list[str] = []
    for rel, data in sorted(files.items()):
        blob = subprocess.run(
            ["git", "-C", str(repo), "hash-object", "-w", "--stdin"],
            input=data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        ).stdout.decode("ascii").strip()
        entries.append(f"100644 blob {blob}\t{rel}")
    tree = subprocess.run(
        ["git", "-C", str(repo), "mktree"],
        input=("\n".join(entries) + "\n").encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout.decode("ascii").strip()
    cmd = ["git", "-C", str(repo), "commit-tree", tree]
    for parent in parents or []:
        cmd.extend(["-p", parent])
    cmd.extend(["-m", message])
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True).stdout.decode("ascii").strip()


def update_ref(repo: Path, ref: str, commit: str, old: str | None = None) -> None:
    full = branch_to_ref(ref)
    cmd = ["update-ref", full, commit]
    if old:
        cmd.append(old)
    git(repo, *cmd)


def mktree_from_entries(repo: Path, entries: list[tuple[str, str, str, str]]) -> str:
    data = "".join(f"{mode} {kind} {oid}\t{name}\n" for mode, kind, oid, name in entries)
    return subprocess.run(
        ["git", "-C", str(repo), "mktree"],
        input=data.encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout.decode("ascii").strip()


def commit_component_skeleton(repo: Path, components: list[dict[str, str]], message: str) -> str:
    """Create a commit whose tree contains component refs at their configured paths."""

    root_entries: dict[str, dict[str, Any]] = {}

    def add_path(parts: list[str], oid: str, node: dict[str, Any]) -> None:
        head = parts[0]
        if len(parts) == 1:
            node[head] = {"tree": oid}
            return
        child = node.setdefault(head, {})
        add_path(parts[1:], oid, child)

    for component in components:
        ref = component["ref"]
        path = component["path"].strip("/")
        if not path:
            raise ReconstructionError("component skeleton paths must not be empty")
        add_path(path.split("/"), tree_id(repo, ref), root_entries)

    def build(node: dict[str, Any]) -> str:
        entries: list[tuple[str, str, str, str]] = []
        for name, value in sorted(node.items()):
            if "tree" in value:
                oid = value["tree"]
            else:
                oid = build(value)
            entries.append(("040000", "tree", oid, name))
        return mktree_from_entries(repo, entries)

    root_tree = build(root_entries)
    return subprocess.run(
        ["git", "-C", str(repo), "commit-tree", root_tree, "-m", message],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout.decode("ascii").strip()


def delta_metadata(repo: Path, base_ref: str, target_ref: str, kind: str, name: str) -> dict[str, Any]:
    gitlinks = []
    for line in git(repo, "ls-tree", "-r", target_ref).stdout.splitlines():
        if line.startswith("160000 "):
            meta, path = line.split("\t", 1)
            _mode, _kind, oid = meta.split()
            gitlinks.append({"path": path, "object_id": oid})
    gitmodules = []
    for line in git(repo, "ls-tree", "-r", target_ref).stdout.splitlines():
        if "\t" in line:
            path = line.split("\t", 1)[1]
            if path == ".gitmodules" or path.endswith("/.gitmodules"):
                gitmodules.append(path)
    return {
        "schema_version": 1,
        "kind": kind,
        "name": name,
        "base_ref": base_ref,
        "base_object_id": rev_parse(repo, base_ref),
        "base_tree_id": tree_id(repo, base_ref),
        "target_ref": target_ref,
        "target_object_id": rev_parse(repo, target_ref),
        "target_tree_id": tree_id(repo, target_ref),
        "gitlinks": gitlinks,
        "gitmodules_paths": gitmodules,
    }


def create_delta_artefact(
    repo: Path,
    base_ref: str,
    target_ref: str,
    artefact_ref: str,
    kind: str,
    name: str,
    message: str,
    allow_replace: bool = False,
) -> str:
    """Create a branch containing metadata.json and delta.patch for base..target."""

    if not ref_exists(repo, base_ref):
        raise ReconstructionError(f"base ref is unavailable: {base_ref}")
    if not ref_exists(repo, target_ref):
        raise ReconstructionError(f"target ref is unavailable: {target_ref}")
    old = rev_parse(repo, artefact_ref) if ref_exists(repo, artefact_ref) else None
    if old and not allow_replace:
        raise ReconstructionError(f"delta artefact ref already exists: {artefact_ref}")
    diff_result = subprocess.run(
        ["git", "-C", str(repo), "diff", "--binary", "--full-index", f"{base_ref}..{target_ref}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if diff_result.returncode not in {0, 1}:
        raise ReconstructionError(diff_result.stderr.decode("utf-8", errors="ignore").strip())
    diff = diff_result.stdout
    metadata = delta_metadata(repo, base_ref, target_ref, kind, name)
    files = {
        "README.md": (
            f"# Delta Artefact: {artefact_ref}\n\n"
            f"Kind: `{kind}`\n\n"
            f"Base: `{base_ref}`\n\n"
            f"Target: `{target_ref}`\n\n"
            "Apply `delta.patch` to the base tree to reproduce the target tree.\n"
        ).encode("utf-8"),
        "metadata.json": json.dumps(metadata, indent=2, sort_keys=True).encode("utf-8") + b"\n",
        "delta.patch": diff,
    }
    commit = commit_tree_with_files(repo, files, message, parents=[old] if old else None)
    update_ref(repo, artefact_ref, commit)
    return commit


def read_delta_artefact_metadata(repo: Path, delta_ref: str) -> dict[str, Any]:
    raw = show_file(repo, delta_ref, "metadata.json")
    return json.loads(raw.decode("utf-8"))


def worktree_paths(repo: Path) -> dict[str, dict[str, str]]:
    result = git(repo, "worktree", "list", "--porcelain")
    entries: dict[str, dict[str, str]] = {}
    current: dict[str, str] = {}
    for line in result.stdout.splitlines():
        if not line:
            if "worktree" in current:
                entries[current["worktree"]] = current
            current = {}
            continue
        key, _, value = line.partition(" ")
        current[key] = value
    if "worktree" in current:
        entries[current["worktree"]] = current
    return entries


def checked_out_worktree(repo: Path, branch: str) -> Path | None:
    full = branch_to_ref(branch)
    for path, data in worktree_paths(repo).items():
        if data.get("branch") == full:
            return Path(path)
    return None


def is_dirty_worktree(path: Path) -> bool:
    result = git(path, "status", "--porcelain", check=True)
    return bool(result.stdout.strip())


def load_ref_records(repo: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    refs_dir = repo / "config" / "refs"
    if not refs_dir.exists():
        return records
    for path in sorted(refs_dir.glob("*.json")):
        data = load_json(repo, f"config/refs/{path.name}")
        for record in data.get("refs", []):
            record = dict(record)
            record.setdefault("manifest", str(path.relative_to(repo)))
            records.append(record)
    return records


def update_ref_record(repo: Path, manifest_name: str, ref: str, updates: dict[str, Any]) -> None:
    """Update or create a config/refs record for a ref moved by an explicit workflow."""

    path = repo / "config" / "refs" / manifest_name
    if path.exists():
        data = load_json(repo, f"config/refs/{manifest_name}")
    else:
        data = {"schema_version": 1, "refs": []}
    records = data.setdefault("refs", [])
    record = None
    for candidate in records:
        if candidate.get("ref") == ref:
            record = candidate
            break
    if record is None:
        record = {"ref": ref}
        records.append(record)
    record.update(updates)
    data["refs"] = sorted(records, key=lambda item: item.get("ref", ""))
    write_json(path, data)


def refresh_ref_record(repo: Path, manifest_name: str, ref: str, extra: dict[str, Any] | None = None) -> None:
    updates: dict[str, Any] = {
        "object_id": rev_parse(repo, ref),
        "tree_id": tree_id(repo, ref),
    }
    if extra:
        updates.update(extra)
    update_ref_record(repo, manifest_name, ref, updates)


def refresh_release_tree(repo: Path, branch: str) -> None:
    path = repo / "config" / "releases.json"
    data = load_json(repo, "config/releases.json")
    entries = data.setdefault("releases", {})
    entry = entries.get(branch) or entries.get(short_release(branch))
    if entry is None:
        raise ReconstructionError(f"cannot update release manifest for unknown release: {branch}")
    entry["tree_id"] = tree_id(repo, branch)
    write_json(path, data)


def immutable_records(repo: Path) -> list[dict[str, Any]]:
    return [r for r in load_ref_records(repo) if r.get("immutable", False)]


def is_immutable_namespace(ref: str) -> bool:
    if ref.startswith("source/base/"):
        return True
    if ref.startswith("source/component/cix/"):
        return True
    if ref.startswith("source/delta/local/"):
        return False
    if ref.startswith("source/delta/"):
        return True
    return False


def immutable_namespace_refs(repo: Path) -> list[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", "refs/heads/source", check=False)
    if result.returncode != 0:
        return []
    return sorted(ref for ref in result.stdout.splitlines() if is_immutable_namespace(ref))


def check_immutable_refs(repo: Path, allow_manifest_update: bool = False, refs: Iterable[str] | None = None) -> None:
    wanted = set(refs or [])
    problems: list[str] = []
    records = immutable_records(repo)
    recorded_refs = {record.get("ref") for record in records if record.get("ref")}
    if not allow_manifest_update and not wanted:
        for ref in immutable_namespace_refs(repo):
            if ref not in recorded_refs:
                problems.append(f"{ref}: immutable namespace ref is not recorded in config/refs/*.json")
    for record in records:
        ref = record.get("ref")
        if not ref or (wanted and ref not in wanted):
            continue
        if not ref_exists(repo, ref):
            # Materialised release refs are generated outputs. A pruned clone may
            # omit them, but any copy that is present is still checked below.
            if str(record.get("type", "")).startswith("rendered-"):
                continue
            if record.get("optional"):
                continue
            problems.append(f"{ref}: missing locally")
            continue
        expected_oid = record.get("object_id")
        expected_tree = record.get("tree_id")
        actual_oid = rev_parse(repo, ref)
        actual_tree = tree_id(repo, ref)
        is_rendered_output = str(record.get("type", "")).startswith("rendered-")
        if expected_oid and actual_oid != expected_oid and not allow_manifest_update and not is_rendered_output:
            problems.append(f"{ref}: object moved from {expected_oid} to {actual_oid}")
        if expected_tree and actual_tree != expected_tree and not allow_manifest_update:
            problems.append(f"{ref}: tree moved from {expected_tree} to {actual_tree}")
        wt = checked_out_worktree(repo, ref)
        if wt and is_dirty_worktree(wt):
            problems.append(f"{ref}: checked out in dirty worktree {wt}")
    if problems:
        details = "\n".join(f"  - {p}" for p in problems)
        raise ReconstructionError(f"immutable ref check failed:\n{details}")


def die(message: str, code: int = 2) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(code)


def main_wrapper(fn) -> None:
    try:
        fn()
    except ReconstructionError as exc:
        die(str(exc), 2)
