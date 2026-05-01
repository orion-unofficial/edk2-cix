#!/usr/bin/env python3
"""Resolve and optionally materialise a configured firmware variant branch."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    cache_dir,
    check_immutable_refs,
    commit_component_skeleton,
    git,
    load_json,
    main_wrapper,
    read_delta_artefact_metadata,
    refresh_ref_record,
    refresh_release_tree,
    ref_exists,
    release_entry,
    repo_root,
    resolve_branch_or_origin,
    rev_parse,
    safe_name,
    show_file,
    short_release,
    temp_dir,
    tree_id,
    truthy,
    update_ref,
    variant_name,
)


HELP = """render-release-branch

Required variables:
  RELEASE    Firmware variant name from 'make help-variants', or a full
             source/release/... branch name.

Optional variables:
  PERSIST=0|1
             Create the rendered source/release/... branch if it is missing.
  REBUILD=0|1
             Regenerate the variant from its render plan.
  FORCE=0|1  With PERSIST=1 and REBUILD=1, intentionally replace the rendered
             branch and refresh config/refs/rendered.json plus releases.json.
  V=0|1      Print delegated git operations and warnings.

Example:
  make render-release-branch \\
    RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1 \\
    PERSIST=1
"""


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


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--release", default=os.environ.get("RELEASE", ""), help="firmware variant name or source/release/... branch")
    p.add_argument("--require-release", action="store_true", help="fail if --release is empty instead of using the default")
    p.add_argument("--persist", default=os.environ.get("PERSIST", "0"), help="create a persistent source/release branch when set to 1")
    p.add_argument("--rebuild", default=os.environ.get("REBUILD", "0"), help="regenerate from render plan instead of reusing an existing branch")
    p.add_argument("--force", default=os.environ.get("FORCE", "0"), help="allow replacing an existing rendered branch with a different tree")
    p.add_argument("--ensure-worktree", action="store_true", help="create or reuse a detached worktree for the resolved variant")
    p.add_argument("--print-worktree", action="store_true", help="print only the worktree path")
    p.add_argument("--print-ref", action="store_true", help="print the resolved ref/branch")
    p.add_argument("--print-default-release", action="store_true", help="print the configured default variant")
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def ensure_worktree(repo: Path, branch: str, target_ref: str, verbose: bool) -> Path:
    commit = rev_parse(repo, target_ref)
    root = cache_dir(repo, "worktrees")
    path = root / f"{safe_name(branch)}-{commit[:12]}"
    if path.exists():
        ignore_worktree_cache(path)
        status = git(path, "status", "--porcelain").stdout.strip()
        if status:
            raise ReconstructionError(f"cached worktree is dirty: {path}")
        return path
    if verbose:
        print(f"Creating detached release worktree {path} at {commit}", file=sys.stderr)
    git(repo, "worktree", "add", "--detach", str(path), commit, capture=not verbose)
    ignore_worktree_cache(path)
    return path


def ensure_base_ref(repo: Path, entry: dict, verbose: bool) -> str | None:
    render = entry.get("render", {})
    base = render.get("base", {})
    base_ref = base.get("ref")
    components = base.get("components", [])
    if not base_ref:
        return None
    if ref_exists(repo, base_ref):
        return base_ref
    releases = load_json(repo, "config/releases.json").get("releases", {})
    if base_ref in releases:
        if verbose:
            print(f"Rendering prerequisite release {base_ref}", file=sys.stderr)
        return render_from_plan(repo, base_ref, releases[base_ref], verbose)
    rendered_base_records = {
        record.get("ref"): record
        for record in load_json(repo, "config/refs/rendered-base.json").get("refs", [])
        if record.get("ref")
    }
    if base_ref in rendered_base_records:
        record = rendered_base_records[base_ref]
        record_components = record.get("components", [])
        if not record_components:
            raise ReconstructionError(f"rendered base ref is missing component metadata: {base_ref}")
        if verbose:
            print(f"Creating rendered base skeleton {base_ref}", file=sys.stderr)
        commit = commit_component_skeleton(
            repo,
            record_components,
            f"base: render EDK2 component skeleton {base_ref.rsplit('/', 1)[-1]}",
        )
        expected_tree = record.get("tree_id")
        actual_tree = tree_id(repo, commit)
        if expected_tree and actual_tree != expected_tree:
            raise ReconstructionError(
                f"generated rendered base tree differs for {base_ref}: {actual_tree} != {expected_tree}"
            )
        update_ref(repo, base_ref, commit)
        return base_ref
    if not components:
        raise ReconstructionError(f"render base ref is missing and has no component plan: {base_ref}")
    if verbose:
        print(f"Creating component skeleton {base_ref}", file=sys.stderr)
    commit = commit_component_skeleton(repo, components, f"base: render component skeleton for {base_ref}")
    update_ref(repo, base_ref, commit)
    return base_ref


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
    git(worktree, "rm", "-r", "--ignore-unmatch", "--", clean_path, capture=not verbose)
    git(worktree, "read-tree", f"--prefix={clean_path}/", "-u", ref, capture=not verbose)


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
    for gitmodules in sorted(worktree.rglob(".gitmodules")):
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
    else:
        for delta_ref in render.get("deltas", []):
            trailers.append(f"Source-Delta: {delta_ref}")
        for component in render.get("component_replacements", []):
            trailers.append(f"Source-Component: {component['path']}={component['ref']}")
    full_message = message + "\n\n" + "\n".join(trailers)
    git(worktree, "commit", "-m", full_message, capture=not verbose)
    return rev_parse(worktree, "HEAD")


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
                for step in steps:
                    if "delta" in step:
                        apply_patch_artefact(repo, worktree, step["delta"], verbose)
                    elif "component" in step:
                        component = step["component"]
                        replace_component_path(repo, worktree, component["path"], component["ref"], verbose)
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
            status = git(worktree, "status", "--porcelain").stdout.strip()
            if not status:
                git(worktree, "commit", "--allow-empty", "-m", f"render: {short_release(branch)}", capture=not verbose)
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


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)

    releases = __import__("reconstruction_common").load_json(repo, "config/releases.json")
    if args.print_default_release:
        print(variant_name(releases.get("default_release", "")))
        return

    if args.require_release and not args.release:
        print(HELP, file=sys.stderr)
        print("missing required variable(s): RELEASE", file=sys.stderr)
        raise SystemExit(2)

    branch, entry = release_entry(repo, args.release or None, require=args.require_release)
    rebuild = truthy(args.rebuild)
    allow_manifest_refresh = truthy(args.persist) and rebuild and truthy(args.force)
    target_ref = None if rebuild else resolve_branch_or_origin(repo, branch, verbose=verbose)
    source_ref = entry.get("source_ref")

    if target_ref is None and not rebuild and source_ref and ref_exists(repo, source_ref):
        target_ref = source_ref

    if target_ref is None and entry.get("render"):
        target_ref = render_from_plan(repo, branch, entry, verbose, allow_manifest_refresh=allow_manifest_refresh)

    if target_ref is None:
        raise ReconstructionError(
            f"firmware variant branch {branch} is unavailable locally and could not be fetched from origin.\n"
            "External upstream/vendor remotes are not contacted for ordinary rendering; "
            "run integrate-source-release if source objects are missing."
        )

    check_immutable_refs(repo)

    if truthy(args.persist):
        if ref_exists(repo, branch):
            existing = rev_parse(repo, branch)
            target = rev_parse(repo, target_ref)
            if existing != target and tree_id(repo, existing) != tree_id(repo, target) and not truthy(args.force):
                raise ReconstructionError(
                    f"persistent branch {branch} already exists at {existing}, not {target}; "
                    "set FORCE=1 if replacing the rendered tree is intentional"
                )
            if existing != target and truthy(args.force):
                update_ref(repo, branch, target, old=existing)
                target_ref = branch
            elif existing != target:
                target_ref = branch
        else:
            if verbose:
                print(f"Creating persistent branch {branch} from {target_ref}", file=sys.stderr)
            git(repo, "branch", branch, target_ref, capture=not verbose)
            target_ref = branch
        if allow_manifest_refresh:
            refresh_ref_record(
                repo,
                "rendered.json",
                branch,
                {
                    "immutable": True,
                    "type": "rendered-release",
                },
            )
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
        print(f"variant: {variant_name(branch)}")
        print(f"branch:  {branch}")
        print(f"ref:     {branch if ref_exists(repo, branch) else target_ref}")
        if wt:
            print(f"worktree:{wt}")


if __name__ == "__main__":
    main_wrapper(main)
