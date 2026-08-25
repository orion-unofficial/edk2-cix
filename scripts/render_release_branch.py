#!/usr/bin/env python3
"""Resolve and optionally materialise a configured source target branch."""

from __future__ import annotations

import argparse
import json
import os
import re
import stat
import sys
import time
from pathlib import Path

from import_workflow import ZERO_OID, transaction_update_refs
from reconstruction_common import (
    ReconstructionError,
    branch_to_ref,
    cache_dir,
    check_immutable_refs,
    clear_metadata_caches,
    commit_component_skeleton,
    format_duration,
    git,
    main_wrapper,
    read_delta_artefact_metadata,
    default_release,
    entry_can_use_source_ref_directly,
    refresh_release_tree,
    ref_exists,
    release_entries,
    release_entry,
    render_base_tree_commit,
    resolve_ref,
    repo_root,
    rev_parse,
    safe_name,
    show_file,
    short_release,
    temp_dir,
    tree_id,
    truthy,
    update_ref,
    source_target_name,
)
from source_policy import enforce_source_tree_policy


HELP = """render-release-branch

Required variables:
  RELEASE    Firmware source target name from 'make help-source-targets', or a full
             source/cache/release/... branch name.

Optional variables:
  PERSIST=0|1
             Create the rendered source/cache/release/... branch if it is missing.
  REBUILD=0|1
             Regenerate the source target from its render plan.
  FORCE=0|1  With PERSIST=1 and REBUILD=1, intentionally replace the rendered
             branch and refresh config/refs-source-target-cache.json.
  V=0|1      Print delegated git operations and warnings.

Example:
  make render-release-branch \\
    RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1 \\
    PERSIST=1
"""


RENDER_COMMIT_IDENTITY = (
    "-c",
    "user.name=EDK2 CIX renderer",
    "-c",
    "user.email=edk2-cix-renderer",
)


def ignore_worktree_cache(worktree: Path) -> None:
    exclude = git(worktree, "rev-parse", "--git-path", "info/exclude").stdout.strip()
    exclude_path = worktree / exclude if not os.path.isabs(exclude) else Path(exclude)
    exclude_text = exclude_path.read_text(encoding="utf-8") if exclude_path.exists() else ""
    if ".cache/" in exclude_text.splitlines():
        return
    exclude_path.parent.mkdir(parents=True, exist_ok=True)
    with exclude_path.open("a", encoding="utf-8") as fh:
        if exclude_text and not exclude_text.endswith("\n"):
            fh.write("\n")
        fh.write(".cache/\n")


def raw_file_matches_index(worktree: Path, relative: str) -> bool:
    stage = git(worktree, "ls-files", "--stage", "--", relative).stdout.strip()
    if not stage or "\t" not in stage:
        return False
    metadata, indexed_path = stage.split("\t", 1)
    mode, object_id, index_stage = metadata.split()
    if indexed_path != relative or index_stage != "0" or mode not in {"100644", "100755"}:
        return False

    path = worktree / relative
    if not path.is_file() or path.is_symlink():
        return False
    executable = bool(path.stat().st_mode & stat.S_IXUSR)
    if executable != (mode == "100755"):
        return False

    raw_id = git(worktree, "hash-object", "--no-filters", "--", relative).stdout.strip()
    return raw_id == object_id


def cached_worktree_is_dirty(worktree: Path) -> bool:
    if git(worktree, "diff", "--cached", "--quiet", "--", check=False).returncode:
        return True
    if git(worktree, "ls-files", "--deleted", "-z").stdout:
        return True
    if git(worktree, "ls-files", "--others", "--exclude-standard", "-z").stdout:
        return True

    modified = [
        path
        for path in git(worktree, "ls-files", "--modified", "-z").stdout.split("\0")
        if path
    ]
    return any(not raw_file_matches_index(worktree, path) for path in modified)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--release", default=os.environ.get("RELEASE", ""), help="source target name or source/cache/release/... branch")
    p.add_argument("--require-release", action="store_true", help="fail if --release is empty instead of using the default")
    p.add_argument("--persist", default=os.environ.get("PERSIST", "0"), help="create a persistent source/cache/release branch when set to 1")
    p.add_argument("--rebuild", default=os.environ.get("REBUILD", "0"), help="regenerate from render plan instead of reusing an existing branch")
    p.add_argument("--force", default=os.environ.get("FORCE", "0"), help="allow replacing an existing rendered branch with a different tree")
    p.add_argument("--ensure-worktree", action="store_true", help="create or reuse a detached worktree for the resolved source target")
    p.add_argument("--print-worktree", action="store_true", help="print only the worktree path")
    p.add_argument("--print-ref", action="store_true", help="print the resolved ref/branch")
    p.add_argument("--print-default-release", action="store_true", help="print the derived default source target")
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def ensure_worktree(repo: Path, branch: str, target_ref: str, verbose: bool) -> Path:
    commit = rev_parse(repo, target_ref)
    root = cache_dir(repo, "worktrees")
    path = root / f"{safe_name(branch)}-{commit[:12]}"
    if path.exists():
        ignore_worktree_cache(path)
        if cached_worktree_is_dirty(path):
            raise ReconstructionError(f"cached worktree is dirty: {path}")
        return path
    if verbose:
        print(f"Creating detached release worktree {path} at {commit}", file=sys.stderr)
    result = git(repo, "worktree", "add", "--detach", str(path), commit)
    if verbose:
        if result.stdout:
            print(result.stdout, end="", file=sys.stderr)
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
    ignore_worktree_cache(path)
    return path


def ensure_base_ref(repo: Path, entry: dict, verbose: bool) -> str | None:
    render = entry.get("render", {})
    base = render.get("base", {})
    base_ref = base.get("ref")
    components = base.get("components", [])
    if not base_ref:
        return None
    resolved_base_ref = resolve_ref(repo, base_ref, check=False)
    if resolved_base_ref:
        return resolved_base_ref
    releases = release_entries(repo)
    if base_ref in releases:
        if verbose:
            print(f"Rendering prerequisite release {base_ref}", file=sys.stderr)
        return render_from_plan(repo, base_ref, releases[base_ref], verbose)
    if base_ref.startswith("source/cache/base/edk2/"):
        if verbose:
            print(f"Creating rendered base skeleton {base_ref}", file=sys.stderr)
        return render_base_tree_commit(repo, base_ref)
    if not components:
        raise ReconstructionError(f"render base ref is missing and has no component plan: {base_ref}")
    if verbose:
        print(f"Creating component skeleton {base_ref}", file=sys.stderr)
    return commit_component_skeleton(repo, components, f"base: render component skeleton for {base_ref}")


def apply_patch_artefact(repo: Path, worktree: Path, delta_ref: str, verbose: bool) -> None:
    metadata = read_delta_artefact_metadata(repo, delta_ref)
    patch = show_file(repo, delta_ref, "delta.patch")
    with temp_dir(repo, "patch-") as tmp:
        patch_path = Path(tmp) / "delta.patch"
        patch_path.write_bytes(patch)
        if verbose:
            print(
                f"Applying {delta_ref} ({metadata.get('base_ref')}..{metadata.get('target_ref')})",
                file=sys.stderr,
            )
        git(worktree, "apply", "--index", "--binary", "--whitespace=nowarn", str(patch_path), capture=not verbose)


def replace_component_path(repo: Path, worktree: Path, path: str, ref: str, verbose: bool) -> None:
    clean_path = path.strip("/")
    if not clean_path:
        raise ReconstructionError("component replacement path must not be empty")
    if verbose:
        print(f"Replacing {clean_path} from {ref}", file=sys.stderr)
    resolved_ref = resolve_ref(repo, ref)
    git(worktree, "rm", "-r", "--ignore-unmatch", "--", clean_path, capture=not verbose)
    git(worktree, "read-tree", f"--prefix={clean_path}/", "-u", resolved_ref, capture=not verbose)


def overlay_paths_from_ref(repo: Path, worktree: Path, ref: str, paths: list[str], missing: str, verbose: bool) -> None:
    resolved_ref = resolve_ref(repo, ref)
    for path in paths:
        clean_path = path.strip("/")
        if not clean_path:
            raise ReconstructionError("overlay path must not be empty")
        path_check = git(repo, "cat-file", "-e", f"{resolved_ref}:{clean_path}", check=False)
        if path_check.returncode != 0:
            ref_check = git(repo, "cat-file", "-e", f"{resolved_ref}^{{tree}}", check=False)
            if ref_check.returncode != 0:
                detail = (ref_check.stderr or ref_check.stdout or "unknown Git error").strip()
                raise ReconstructionError(f"could not inspect overlay source {ref}: {detail}")
            if missing == "ignore":
                continue
            raise ReconstructionError(f"overlay path is missing from {ref}: {clean_path}")
        if verbose:
            print(f"Overlaying {clean_path} from {ref}", file=sys.stderr)
        git(worktree, "rm", "-r", "--ignore-unmatch", "--", clean_path, capture=not verbose)
        git(worktree, "checkout", resolved_ref, "--", clean_path, capture=not verbose)


def apply_release_metadata(repo: Path, worktree: Path, ref: str, release: str, verbose: bool) -> None:
    """Take package identity from the selected Radxa release source."""

    overlay_paths_from_ref(
        repo,
        worktree,
        ref,
        ["debian/changelog"],
        "error",
        verbose,
    )
    version = worktree / "VERSION"
    expected = f"{release}\n"
    if not version.exists() or version.read_text(encoding="utf-8") != expected:
        if verbose:
            print(f"Setting VERSION to Radxa {release}", file=sys.stderr)
        version.write_text(expected, encoding="utf-8")
        git(worktree, "add", "--", "VERSION", capture=not verbose)


def make_prereqs_optional(worktree: Path, path: str, variable: str, verbose: bool) -> None:
    clean_path = path.strip("/")
    if not clean_path:
        raise ReconstructionError("make prerequisite path must not be empty")
    makefile = worktree / clean_path
    if not makefile.exists():
        raise ReconstructionError(f"make prerequisite file does not exist: {clean_path}")
    lines = makefile.read_text(encoding="utf-8").splitlines(keepends=True)
    in_variable = False
    changed = False
    rewritten: list[str] = []
    pattern = re.compile(r"^(\s*)\$\((abspath\s+[^)]+)\)(\s*\\?\s*)$")
    for line in lines:
        stripped = line.strip()
        if stripped.startswith(f"{variable} :="):
            in_variable = True
            rewritten.append(line)
            continue
        if in_variable:
            match = pattern.match(line.rstrip("\n"))
            if match and "$(wildcard " not in line:
                newline = "\n" if line.endswith("\n") else ""
                rewritten.append(f"{match.group(1)}$(wildcard $({match.group(2)})){match.group(3)}{newline}")
                changed = True
            else:
                rewritten.append(line)
            if not line.rstrip().endswith("\\"):
                in_variable = False
            continue
        rewritten.append(line)
    if changed:
        if verbose:
            print(f"Relaxing missing {variable} prerequisites in {clean_path}", file=sys.stderr)
        makefile.write_text("".join(rewritten), encoding="utf-8")
        git(worktree, "add", "--", clean_path, capture=not verbose)


def add_dependency_package(worktree: Path, path: str, after: str, package: str, verbose: bool) -> None:
    clean_path = path.strip("/")
    if not clean_path:
        raise ReconstructionError("dependency compatibility path must not be empty")
    script = worktree / clean_path
    if not script.exists():
        raise ReconstructionError(f"dependency compatibility file does not exist: {clean_path}")
    text = script.read_text(encoding="utf-8")
    if re.search(rf"^\s*{re.escape(package)}\s*$", text, flags=re.MULTILINE):
        return
    marker = f"    {after}\n"
    if marker not in text:
        raise ReconstructionError(f"could not locate dependency insertion point {after!r} in {clean_path}")
    if verbose:
        print(f"Adding dependency compatibility package {package} to {clean_path}", file=sys.stderr)
    script.write_text(text.replace(marker, f"{marker}    {package}\n", 1), encoding="utf-8")
    git(worktree, "add", "--", clean_path, capture=not verbose)


def add_vendor_asl_compat(worktree: Path, path: str, messages: list[str], verbose: bool) -> None:
    clean_path = path.strip("/")
    if not clean_path:
        raise ReconstructionError("ASL compatibility path must not be empty")
    makefile = worktree / clean_path
    if not makefile.exists():
        raise ReconstructionError(f"ASL compatibility file does not exist: {clean_path}")
    flags = " ".join(f"-vw {message}" for message in messages)
    text = makefile.read_text(encoding="utf-8")
    if flags in text:
        return
    old = "\t\t\tupstream) \\\n\t\t\t\t;; \\\n"
    new = (
        "\t\t\tupstream) \\\n"
        "\t\t\t\ttool_def_overrides+=( \\\n"
        f"\t\t\t\t\t'  DEBUG_GCC5_AARCH64_ASL_FLAGS   = {flags}' \\\n"
        f"\t\t\t\t\t'RELEASE_GCC5_AARCH64_ASL_FLAGS   = {flags}' \\\n"
        "\t\t\t\t); \\\n"
        "\t\t\t\t;; \\\n"
    )
    if old not in text:
        raise ReconstructionError(f"could not locate upstream ASL flag block in {clean_path}")
    if verbose:
        print(f"Adding historical vendor ASL compatibility flags to {clean_path}", file=sys.stderr)
    makefile.write_text(text.replace(old, new, 1), encoding="utf-8")
    git(worktree, "add", "--", clean_path, capture=not verbose)


def gitlinks(worktree: Path) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    for line in git(worktree, "ls-files", "-s").stdout.splitlines():
        if not line.startswith("160000 "):
            continue
        meta, path = line.split("\t", 1)
        mode, oid, stage = meta.split()
        entries.append({"mode": mode, "kind": "commit", "object_id": oid, "stage": stage, "path": path})
    return entries


def parse_gitmodules(worktree: Path) -> dict[str, dict[str, str]]:
    mappings: dict[str, dict[str, str]] = {}
    result = git(worktree, "ls-files", "-z", "--", ".gitmodules", ":(glob)**/.gitmodules", check=False)
    if result.returncode != 0:
        return mappings
    gitmodules_paths = sorted(path for path in result.stdout.split("\0") if path)
    for relative in gitmodules_paths:
        gitmodules = worktree / relative
        result = git(
            worktree,
            "config",
            "-f",
            str(gitmodules),
            "--get-regexp",
            r"^submodule\..*\.(path|url)$",
            check=False,
        )
        if result.returncode != 0:
            continue
        records: dict[str, dict[str, str]] = {}
        for line in result.stdout.splitlines():
            key, _, value = line.partition(" ")
            parts = key.split(".")
            if len(parts) < 3:
                continue
            name = ".".join(parts[1:-1])
            field = parts[-1]
            records.setdefault(name, {})[field] = value.strip()
        base = gitmodules.parent
        for record in records.values():
            if "path" not in record:
                continue
            rel = (base / record["path"]).resolve().relative_to(worktree.resolve())
            mappings[str(rel)] = {
                "url": record.get("url", ""),
                "gitmodules": str(gitmodules.relative_to(worktree)),
            }
    return mappings


def fetch_submodule_commit(repo: Path, url: str, oid: str, verbose: bool) -> None:
    if git(repo, "cat-file", "-e", f"{oid}^{{tree}}", check=False).returncode == 0:
        return
    if not url:
        raise ReconstructionError(f"cannot materialise submodule {oid}: no URL recorded in .gitmodules")
    if verbose:
        print(f"Fetching submodule {oid} from {url}", file=sys.stderr)
    result = git(repo, "fetch", "--no-tags", url, oid, check=False, capture=not verbose)
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "unknown fetch failure").strip()
        raise ReconstructionError(f"could not fetch submodule {oid} from {url}: {detail}")


def materialise_submodules(repo: Path, worktree: Path, branch: str, verbose: bool) -> list[dict[str, str]]:
    report: list[dict[str, str]] = []
    while True:
        links = gitlinks(worktree)
        if not links:
            break
        mappings = parse_gitmodules(worktree)
        progressed = False
        for link in links:
            path = link["path"]
            mapping = mappings.get(path, {})
            url = mapping.get("url", "")
            fetch_submodule_commit(repo, url, link["object_id"], verbose)
            if verbose:
                print(f"Materialising gitlink {path} -> {link['object_id']}", file=sys.stderr)
            git(worktree, "rm", "-f", "--", path, capture=not verbose)
            git(worktree, "read-tree", f"--prefix={path}/", "-u", link["object_id"], capture=not verbose)
            report.append({
                "path": path,
                "object_id": link["object_id"],
                "url": url,
                "gitmodules": mapping.get("gitmodules", ""),
            })
            progressed = True
        if not progressed:
            unresolved = ", ".join(item["path"] for item in links)
            raise ReconstructionError(f"could not materialise remaining gitlinks: {unresolved}")
    if report:
        report_dir = cache_dir(repo, "reports")
        report_path = report_dir / f"{safe_name(branch)}-submodules.json"
        report_path.write_text(json.dumps({"branch": branch, "submodules": report}, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if verbose:
            print(f"Wrote submodule materialisation report {report_path}", file=sys.stderr)
    return report


def commit_rendered_worktree(repo: Path, worktree: Path, branch: str, entry: dict, verbose: bool) -> str:
    render = entry.get("render", {})
    message = render.get("commit_message") or f"render: {short_release(branch)}"
    trailers = [f"Source-Release: {branch}"]
    base_ref = render.get("base", {}).get("ref")
    if base_ref:
        trailers.append(f"Source-Base: {base_ref}")
    if render.get("steps"):
        for step in render["steps"]:
            if "delta" in step:
                trailers.append(f"Source-Delta: {step['delta']}")
            elif "component" in step:
                component = step["component"]
                trailers.append(f"Source-Component: {component['path']}={component['ref']}")
            elif "overlay_paths" in step:
                overlay = step["overlay_paths"]
                trailers.append(f"Source-Overlay: {','.join(overlay.get('paths', []))}={overlay.get('ref', '')}")
            elif "release_metadata" in step:
                metadata = step["release_metadata"]
                trailers.append(
                    f"Source-Release-Metadata: Radxa {metadata.get('release', '')} from {metadata.get('ref', '')}"
                )
            elif "make_optional_prereqs" in step:
                prereqs = step["make_optional_prereqs"]
                trailers.append(f"Source-Build-Compat: optional {prereqs.get('variable', '')} in {prereqs.get('path', '')}")
            elif "vendor_dependency_compat" in step:
                compat = step["vendor_dependency_compat"]
                trailers.append(
                    f"Source-Build-Compat: dependency {compat.get('package', '')} in {compat.get('path', '')}"
                )
            elif "vendor_asl_compat" in step:
                compat = step["vendor_asl_compat"]
                trailers.append(
                    f"Source-Build-Compat: historical ASL messages {','.join(compat.get('messages', []))} in {compat.get('path', '')}"
                )
    else:
        for delta_ref in render.get("deltas", []):
            trailers.append(f"Source-Delta: {delta_ref}")
        for component in render.get("component_replacements", []):
            trailers.append(f"Source-Component: {component['path']}={component['ref']}")
    full_message = message + "\n\n" + "\n".join(trailers)
    git(worktree, *RENDER_COMMIT_IDENTITY, "commit", "-m", full_message, capture=not verbose)
    return rev_parse(worktree, "HEAD")


def validate_release_metadata(repo: Path, ref: str, entry: dict, branch: str) -> None:
    """Reject unofficial source targets labelled as a different Radxa release."""

    if not entry.get("unofficial_delta"):
        return
    expected = str(entry.get("radxa_release", "")).strip()
    version = show_file(repo, ref, "VERSION").decode("utf-8", errors="replace").strip()
    changelog = show_file(repo, ref, "debian/changelog").decode("utf-8", errors="replace")
    match = re.match(r"^[^ ]+ \(([^)]+)\)", changelog)
    changelog_version = match.group(1) if match else ""
    mismatches = []
    if version != expected:
        mismatches.append(f"VERSION={version or '<missing>'}")
    if changelog_version != expected:
        mismatches.append(f"debian/changelog={changelog_version or '<invalid>'}")
    if mismatches:
        raise ReconstructionError(
            f"release metadata for {branch} does not match Radxa {expected}: "
            + ", ".join(mismatches)
        )


def render_from_plan(repo: Path, branch: str, entry: dict, verbose: bool, allow_manifest_refresh: bool = False) -> str:
    render = entry.get("render")
    if not render:
        raise ReconstructionError(f"release has no render plan: {branch}")
    base_ref = ensure_base_ref(repo, entry, verbose)
    if not base_ref:
        raise ReconstructionError(f"release render plan has no base ref: {branch}")

    with temp_dir(repo, f"render-{safe_name(branch)}-") as tmp:
        worktree = Path(tmp) / "worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), base_ref, capture=not verbose)
        try:
            steps = render.get("steps")
            if steps:
                for step_index, step in enumerate(steps, start=1):
                    print(
                        f"[render] Applying {branch} step {step_index}/{len(steps)}: "
                        f"{next(iter(step))}",
                        file=sys.stderr,
                        flush=True,
                    )
                    if "delta" in step:
                        apply_patch_artefact(repo, worktree, step["delta"], verbose)
                    elif "component" in step:
                        component = step["component"]
                        replace_component_path(repo, worktree, component["path"], component["ref"], verbose)
                    elif "overlay_paths" in step:
                        overlay = step["overlay_paths"]
                        overlay_paths_from_ref(
                            repo,
                            worktree,
                            overlay["ref"],
                            overlay.get("paths", []),
                            overlay.get("missing", "error"),
                            verbose,
                        )
                    elif "release_metadata" in step:
                        metadata = step["release_metadata"]
                        apply_release_metadata(
                            repo,
                            worktree,
                            metadata["ref"],
                            metadata["release"],
                            verbose,
                        )
                    elif "make_optional_prereqs" in step:
                        prereqs = step["make_optional_prereqs"]
                        make_prereqs_optional(
                            worktree,
                            prereqs["path"],
                            prereqs["variable"],
                            verbose,
                        )
                    elif "vendor_dependency_compat" in step:
                        compat = step["vendor_dependency_compat"]
                        add_dependency_package(
                            worktree,
                            compat["path"],
                            compat["after"],
                            compat["package"],
                            verbose,
                        )
                    elif "vendor_asl_compat" in step:
                        compat = step["vendor_asl_compat"]
                        add_vendor_asl_compat(
                            worktree,
                            compat["path"],
                            compat.get("messages", []),
                            verbose,
                        )
                    elif step.get("materialise_submodules"):
                        materialise_submodules(repo, worktree, branch, verbose)
                    else:
                        raise ReconstructionError(f"unknown render step: {step}")
            else:
                for delta_ref in render.get("deltas", []):
                    apply_patch_artefact(repo, worktree, delta_ref, verbose)
                for component in render.get("component_replacements", []):
                    replace_component_path(repo, worktree, component["path"], component["ref"], verbose)
            if render.get("remove_root_gitmodules", True) and (worktree / ".gitmodules").exists():
                git(worktree, "rm", "-f", ".gitmodules", capture=not verbose)
            if gitlinks(worktree):
                materialise_submodules(repo, worktree, branch, verbose)
            enforce_source_tree_policy(worktree, index=True, label=branch)
            status = git(worktree, "status", "--porcelain").stdout.strip()
            has_staged_changes = git(worktree, "diff", "--cached", "--quiet", check=False).returncode != 0
            if not status:
                git(
                    worktree,
                    *RENDER_COMMIT_IDENTITY,
                    "commit",
                    "--allow-empty",
                    "-m",
                    f"render: {short_release(branch)}",
                    capture=not verbose,
                )
                commit = rev_parse(worktree, "HEAD")
            elif not has_staged_changes:
                commit = rev_parse(worktree, "HEAD")
            else:
                commit = commit_rendered_worktree(repo, worktree, branch, entry, verbose)
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False, capture=True)
    expected_tree = entry.get("tree_id")
    if expected_tree and tree_id(repo, commit) != expected_tree and not allow_manifest_refresh:
        raise ReconstructionError(
            f"rendered tree for {branch} does not match manifest: {tree_id(repo, commit)} != {expected_tree}"
        )
    return commit


def coupled_persistent_refs(repo: Path, branch: str) -> list[str]:
    """Return an unofficial target and its versioned/canonical counterpart."""
    entries = release_entries(repo)
    canonical = entries.get(branch, {}).get("alias_of", branch)
    return sorted(
        ref
        for ref, entry in entries.items()
        if ref == canonical or entry.get("alias_of") == canonical
    ) or [branch]


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    started = time.monotonic()

    if args.print_default_release:
        print(source_target_name(default_release(repo)))
        return

    if args.require_release and not args.release:
        print(HELP, file=sys.stderr)
        print("missing required variable(s): RELEASE", file=sys.stderr)
        raise SystemExit(2)

    branch, entry = release_entry(repo, args.release or None, require=args.require_release)
    rebuild = truthy(args.rebuild)
    allow_manifest_refresh = truthy(args.persist) and rebuild and truthy(args.force)
    target_ref = None if rebuild or not ref_exists(repo, branch) else branch
    source_ref = entry.get("source_ref")

    if (
        target_ref is None
        and not rebuild
        and source_ref
        and ref_exists(repo, source_ref)
        and entry_can_use_source_ref_directly(entry)
    ):
        target_ref = source_ref

    if target_ref is None and entry.get("render"):
        target_ref = render_from_plan(repo, branch, entry, verbose, allow_manifest_refresh=allow_manifest_refresh)

    if target_ref is None:
        raise ReconstructionError(
            f"source target branch {branch} is unavailable locally and could not be regenerated.\n"
            "External upstream/vendor remotes are not contacted for ordinary rendering; "
            "run integrate-source-release if source objects are missing."
        )

    validate_release_metadata(repo, target_ref, entry, branch)

    expected_tree = entry.get("tree_id")
    if expected_tree and tree_id(repo, target_ref) != expected_tree and not allow_manifest_refresh:
        raise ReconstructionError(
            f"resolved tree for {branch} does not match manifest: {tree_id(repo, target_ref)} != {expected_tree}"
        )

    print("[render] Validating immutable source refs", file=sys.stderr, flush=True)
    check_immutable_refs(
        repo,
        allow_generated_refresh=allow_manifest_refresh,
    )

    if truthy(args.persist):
        target = rev_parse(repo, target_ref)
        if allow_manifest_refresh:
            refs = [
                ref
                for ref in coupled_persistent_refs(repo, branch)
                if ref == branch or ref_exists(repo, ref)
            ]
            updates = [
                (
                    branch_to_ref(ref),
                    target,
                    rev_parse(repo, ref) if ref_exists(repo, ref) else ZERO_OID,
                )
                for ref in refs
                if not ref_exists(repo, ref) or rev_parse(repo, ref) != target
            ]
            if verbose:
                for full_ref, new_oid, old_oid in updates:
                    print(f"Updating persistent branch {full_ref}: {old_oid} -> {new_oid}", file=sys.stderr)
            transaction_update_refs(repo, updates)
            target_ref = branch
        elif ref_exists(repo, branch):
            existing = rev_parse(repo, branch)
            if existing != target and tree_id(repo, existing) != tree_id(repo, target) and not truthy(args.force):
                raise ReconstructionError(
                    f"persistent branch {branch} already exists at {existing}, not {target}; "
                    "set FORCE=1 if replacing the rendered tree is intentional"
                )
            if existing != target and truthy(args.force):
                update_ref(repo, branch, target, old=existing)
                clear_metadata_caches()
                target_ref = branch
            elif existing != target:
                target_ref = branch
        elif not allow_manifest_refresh:
            if verbose:
                print(f"Creating persistent branch {branch} from {target_ref}", file=sys.stderr)
            git(repo, "branch", branch, target_ref, capture=not verbose)
            clear_metadata_caches()
            target_ref = branch
        if allow_manifest_refresh:
            refresh_release_tree(repo, branch)

    wt: Path | None = None
    if args.ensure_worktree:
        wt = ensure_worktree(repo, branch, target_ref, verbose=verbose)

    if args.print_worktree:
        if wt is None:
            wt = ensure_worktree(repo, branch, target_ref, verbose=verbose)
        print(wt)
    elif args.print_ref:
        print(branch if ref_exists(repo, branch) else target_ref)
    else:
        print(f"source target: {source_target_name(branch)}")
        print(f"branch:  {branch}")
        print(f"ref:     {branch if ref_exists(repo, branch) else target_ref}")
        if wt:
            print(f"worktree:{wt}")
    print(f"[render] Prepared {branch} in {format_duration(time.monotonic() - started)}", file=sys.stderr)


if __name__ == "__main__":
    main_wrapper(main)
