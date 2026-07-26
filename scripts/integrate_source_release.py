#!/usr/bin/env python3
"""Integrate new upstream or vendor source refs into the source model."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from reconstruction_common import (
    CACHE_BASE_EDK2_PREFIX,
    ARM_REFS_MANIFEST,
    CIX_REFS_MANIFEST,
    EDK2_REFS_MANIFEST,
    RADXA_SOURCE_RE,
    RADXA_REFS_MANIFEST,
    ReconstructionError,
    check_immutable_refs,
    clear_metadata_caches,
    configured_remote_url,
    for_each_ref,
    git,
    load_json,
    load_ref_records,
    main_wrapper,
    ref_exists,
    repo_root,
    resolve_ref,
    resolve_ref_or_generated_cache,
    tree_id,
    truthy,
    rev_parse,
    temp_dir,
    version_key,
)
from render_release_branch import gitlinks, materialise_submodules
from source_porting import apply_source_delta_to_base, normalise_source_tree


HELP = """integrate-source-release

Supported forms:
  make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202605 WRITE=1
  make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7 WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2 WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2 COMPONENT=tf-a ARM_BASE=v2.7 REF=<ported-ref> WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 REF=<vendor-ref> WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202605 REF=<ported-ref> RADXA_SOURCE=port WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1+<commit> EDK2_BASE=edk2-stable202208 REF=main WRITE=1

Required variables:
  TYPE=upstream|vendor
  COMPONENT=edk2|edk2-platforms|edk2-non-osi|tf-a|op-tee when TYPE=upstream
  VENDOR=radxa|cix when TYPE=vendor

Optional variables:
  RELEASE=<release-or-ref>  Release tag/version to integrate.
  EDK2_BASE=<release>       EDK2 base marker for Radxa source refs.
  FROM_EDK2_BASE=<release>  Previous EDK2 base used to port a Radxa source
                            tree when REF is omitted.
  ARM_BASE=<release>        Arm base marker for CIX component uplift refs.
  REF=<object-id-or-ref>    Explicit object to use instead of a configured remote tag.
  RADXA_SOURCE=auto|vendor|port
                            For Radxa, record an actual vendor release source or
                            this project's port of that vendor release to another
                            EDK2 base. auto records the first Radxa source for a
                            release under source/vendor/radxa/** and later EDK2
                            bases under source/port/radxa/**.
  WRITE=0|1                 Required before refs are created or advanced.
  ALLOW_REPLACE=0|1         Allow an existing immutable ref to move during integration.
  MATERIALISE=0|1           For Radxa source refs, flatten gitlinks before recording.
  V=0|1                     Print delegated git operations.

Without WRITE=1 this command validates inputs and prints the operation it would perform.
Only this command is allowed to create or advance immutable source refs.
If an immutable target is already recorded locally or as an origin
remote-tracking ref, rerunning the same integration is a no-op. The command
reports where the existing ref was found; git branch alone lists only local
branches.
If the requested object is already present locally, WRITE=1 uses it without
contacting the external upstream/vendor remote.
Radxa non-release updates should use a descriptive RELEASE such as
1.2.1+<short-commit>. REF may point at a legacy submodule-shaped branch such
as main; with MATERIALISE=1, the source is flattened before the Radxa source ref
is recorded.
CIX TF-A/OP-TEE uplift experiments should use COMPONENT=tf-a|op-tee,
ARM_BASE=<arm-release>, and REF=<ported-ref>. This records the finished
component ref under source/port/cix/<cix-release>/<component>/<arm-base>.
For edk2-platforms and edk2-non-osi stable-release integration, REF may be
omitted; the script selects the latest upstream master commit at or before the
matching EDK2 stable tag committer timestamp.
For Radxa ports to a newer EDK2 base, omit REF and provide FROM_EDK2_BASE to
replay the previous Radxa source delta onto the new EDK2 base.
"""

UPSTREAM_COMPONENTS = {"edk2", "edk2-platforms", "edk2-non-osi", "tf-a", "op-tee"}
EDK2_COMPANION_COMPONENTS = {"edk2-platforms", "edk2-non-osi"}
VENDORS = {"radxa", "cix"}


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--type", dest="type_", default=os.environ.get("TYPE", ""))
    p.add_argument("--component", default=os.environ.get("COMPONENT", ""))
    p.add_argument("--vendor", default=os.environ.get("VENDOR", ""))
    p.add_argument("--release", default=os.environ.get("RELEASE", ""))
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
    p.add_argument("--from-edk2-base", default=os.environ.get("FROM_EDK2_BASE", ""))
    p.add_argument("--arm-base", default=os.environ.get("ARM_BASE", ""))
    p.add_argument("--ref", default=os.environ.get("REF", ""))
    p.add_argument("--radxa-source", default=os.environ.get("RADXA_SOURCE", "auto"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--allow-replace", default=os.environ.get("ALLOW_REPLACE", "0"))
    p.add_argument("--materialise", default=os.environ.get("MATERIALISE", "1"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def upstream_target(component: str, release: str) -> str:
    if component == "edk2":
        return f"source/base/edk2/{release}"
    if component in {"edk2-platforms", "edk2-non-osi"}:
        return f"source/base/{component}/{release}"
    return f"source/base/arm/{component}/{release}"


def normalise_edk2_base(value: str) -> str:
    value = value.strip()
    if not value:
        return value
    if value.startswith("edk2-stable"):
        return value
    if value.startswith("edk2-"):
        return "edk2-stable" + value.removeprefix("edk2-")
    return "edk2-stable" + value


def cache_base_ref(edk2_base: str) -> str:
    return f"{CACHE_BASE_EDK2_PREFIX}{normalise_edk2_base(edk2_base)}"


def commit_timestamp(repo: Path, ref: str) -> str:
    return git(repo, "show", "-s", "--format=%cI", ref).stdout.strip()


def edk2_release_timestamp(repo: Path, release: str, verbose: bool) -> str:
    base_ref = f"source/base/edk2/{release}"
    if ref_exists(repo, base_ref):
        return commit_timestamp(repo, resolve_ref(repo, base_ref))
    tag_ref = f"refs/tags/{release}"
    if ref_exists(repo, tag_ref):
        return commit_timestamp(repo, resolve_ref(repo, tag_ref))
    remote = configured_remote_url(repo, component="edk2", remote_type="upstream")
    if verbose:
        print(f"fetch {remote} {tag_ref} for companion timestamp")
    git(repo, "fetch", "--no-tags", remote, tag_ref, capture=not verbose)
    return commit_timestamp(repo, "FETCH_HEAD")


def selected_master_commit(repo: Path, remote: str, timestamp: str, verbose: bool) -> str:
    if verbose:
        print(f"fetch {remote} refs/heads/master for timestamp selection")
    git(repo, "fetch", "--no-tags", remote, "refs/heads/master", capture=not verbose)
    commit = git(repo, "rev-list", "-n", "1", f"--before={timestamp}", "FETCH_HEAD").stdout.strip()
    if not commit:
        raise ReconstructionError(f"no upstream master commit found at or before {timestamp}")
    return commit


def upstream_operation(
    repo: Path,
    component: str,
    release: str,
    explicit_ref: str,
    verbose: bool,
) -> tuple[str, str, str, dict[str, str]]:
    target = upstream_target(component, release)
    remote = configured_remote_url(repo, component=component, remote_type="upstream")
    if component in EDK2_COMPANION_COMPONENTS and not explicit_ref:
        timestamp = edk2_release_timestamp(repo, release, verbose)
        source = selected_master_commit(repo, remote, timestamp, verbose)
        return (
            remote,
            source,
            target,
            {
                "type": "base",
                "component": component,
                "upstream_ref": "refs/heads/master",
                "selected_at_or_before": timestamp,
            },
        )
    source = explicit_ref or f"refs/tags/{release}"
    return (remote, source, target, {"type": "base", "component": component, "upstream_ref": source})


def cix_component_records(repo: Path, release: str) -> list[dict[str, str]]:
    release = release.removeprefix("v")
    records = []
    for record in load_json(repo, f"config/{CIX_REFS_MANIFEST}").get("refs", []):
        ref = str(record.get("ref", ""))
        if not ref.startswith(f"source/vendor/cix/{release}/"):
            continue
        if record.get("type") not in {"vendor-bundle", "vendor-component", "vendor-payload"}:
            continue
        records.append(record)
    return sorted(records, key=lambda item: str(item.get("ref", "")))


def cix_record_remote(repo: Path, record: dict[str, str]) -> str:
    remote_key = str(record.get("remote", ""))
    if remote_key:
        return configured_remote_url(repo, remote_key=remote_key)
    record_type = str(record.get("type", ""))
    if record_type == "vendor-bundle":
        return configured_remote_url(repo, vendor="cix", remote_type="vendor-bundle")
    return configured_remote_url(repo, component=str(record.get("component", "")), vendor="cix")


def fetch_to_ref(repo: Path, remote: str, source: str, target: str, verbose: bool, allow_replace: bool) -> None:
    if ref_exists(repo, target) and not allow_replace:
        if verbose:
            print(f"{target} already exists; leaving it unchanged")
        return
    if ref_exists(repo, source):
        if verbose:
            print(f"using local object {source} -> {target}")
        if ref_exists(repo, target):
            git(repo, "branch", "-f", target, source, capture=not verbose)
        else:
            git(repo, "branch", target, source, capture=not verbose)
        return
    if verbose:
        print(f"fetch {remote} {source} -> {target}")
    prefix = "+" if allow_replace else ""
    result = git(repo, "fetch", "--no-tags", remote, f"{prefix}{source}:refs/heads/{target}", check=False, capture=not verbose)
    if result.returncode == 0:
        return
    if ref_exists(repo, source):
        if verbose:
            print(f"external fetch failed, but {source} is now available locally; using it", file=sys.stderr)
        if ref_exists(repo, target):
            git(repo, "branch", "-f", target, source, capture=not verbose)
        else:
            git(repo, "branch", target, source, capture=not verbose)
        return
    detail = (result.stderr or result.stdout or "unknown fetch failure").strip()
    raise ReconstructionError(
        f"could not fetch {source} from {remote} for {target}; "
        f"the requested object is not available locally: {detail}"
    )


def manifest_path_for(target: str) -> str:
    if target.startswith(("source/base/edk2/", "source/base/edk2-platforms/", "source/base/edk2-non-osi/")):
        return f"config/{EDK2_REFS_MANIFEST}"
    if target.startswith("source/base/arm/"):
        return f"config/{ARM_REFS_MANIFEST}"
    if target.startswith(("source/vendor/cix/", "source/port/cix/")):
        return f"config/{CIX_REFS_MANIFEST}"
    if target.startswith(("source/vendor/radxa/", "source/port/radxa/")):
        return f"config/{RADXA_REFS_MANIFEST}"
    return "config/refs-integrated.json"


def exact_commit_id(repo: Path, ref: str) -> str | None:
    result = git(repo, "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}", check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def target_ref_copies(repo: Path, target: str) -> list[tuple[str, str]]:
    copies = []
    for ref in (f"refs/heads/{target}", f"refs/remotes/origin/{target}"):
        object_id = exact_commit_id(repo, ref)
        if object_id:
            copies.append((ref, object_id))
    return copies


def manifest_record_for_target(repo: Path, target: str) -> dict | None:
    for record in load_ref_records(repo):
        if record.get("ref") == target:
            return record
    return None


def available_commit_id(repo: Path, ref: str) -> str | None:
    resolved = resolve_ref(repo, ref, check=False)
    if resolved:
        return exact_commit_id(repo, resolved)
    value = ref.lower()
    if len(value) in {40, 64} and all(character in "0123456789abcdef" for character in value):
        return value
    return None


def existing_immutable_target(
    repo: Path,
    target: str,
    source: str,
    meta: dict[str, str],
) -> dict | None:
    copies = target_ref_copies(repo, target)
    if not copies:
        return None

    object_ids = {object_id for _ref, object_id in copies}
    copy_details = ", ".join(f"{ref} -> {object_id}" for ref, object_id in copies)
    if len(object_ids) != 1:
        raise ReconstructionError(
            f"immutable target has inconsistent local and remote-tracking copies: {copy_details}"
        )
    object_id = next(iter(object_ids))

    record = manifest_record_for_target(repo, target)
    if record is None:
        raise ReconstructionError(
            f"immutable target already exists as {copy_details}, but it is not recorded in config/refs-*.json"
        )
    expected_object_id = str(record.get("object_id", ""))
    expected_tree_id = str(record.get("tree_id", ""))
    actual_tree_id = tree_id(repo, object_id)
    if expected_object_id != object_id or expected_tree_id != actual_tree_id:
        raise ReconstructionError(
            f"immutable target metadata does not match {copy_details}; "
            f"manifest records object {expected_object_id or '<missing>'} "
            f"and tree {expected_tree_id or '<missing>'}"
        )

    recorded_upstream = str(record.get("upstream_ref", ""))
    if meta.get("type") == "vendor-source" and recorded_upstream and not source.startswith("<"):
        source_object_id = available_commit_id(repo, source)
        recorded_object_id = available_commit_id(repo, recorded_upstream)
        if not source_object_id:
            raise ReconstructionError(
                f"{target} is already integrated from upstream_ref {recorded_upstream}, "
                f"but requested REF={source} is unavailable locally"
            )
        if recorded_object_id and source_object_id != recorded_object_id:
            raise ReconstructionError(
                f"{target} is already integrated from upstream_ref {recorded_upstream} "
                f"({recorded_object_id}), but requested REF={source} resolves to {source_object_id}; "
                "fetch the matching release tag or use a different RELEASE"
            )

    return {
        "target": target,
        "object_id": object_id,
        "copies": copies,
        "has_local_head": any(ref.startswith("refs/heads/") for ref, _object_id in copies),
    }


def print_existing_target(state: dict, *, dry_run: bool) -> None:
    target = str(state["target"])
    object_id = str(state["object_id"])
    locations = ", ".join(ref for ref, _object_id in state["copies"])
    prefix = "no change" if dry_run else "unchanged"
    print(f"  {prefix}: {target} is already integrated at {object_id}")
    print(f"    available as {locations}")
    if not state["has_local_head"]:
        if dry_run:
            print(f"    WRITE=1 will create the missing local branch {target}")
        else:
            print("    git branch lists only local branches; use git branch -r or git branch -a to see this ref")


def materialise_existing_target_local_head(repo: Path, state: dict, *, verbose: bool) -> bool:
    """Create a local immutable branch from its verified remote-tracking copy."""

    if state["has_local_head"]:
        return False
    target = str(state["target"])
    object_id = str(state["object_id"])
    git(repo, "branch", target, object_id, capture=not verbose)
    clear_metadata_caches()
    state["copies"] = [(f"refs/heads/{target}", object_id), *state["copies"]]
    state["has_local_head"] = True
    print(f"  created local branch: {target} -> {object_id}")
    return True


def radxa_vendor_refs(repo: Path, release: str) -> list[str]:
    return [
        ref
        for ref in for_each_ref(repo, f"source/vendor/radxa/{release}")
        if ref.startswith(f"source/vendor/radxa/{release}/")
    ]


def radxa_source_ref_for_base(repo: Path, release: str, edk2_base: str) -> str:
    edk2_base = normalise_edk2_base(edk2_base)
    candidates = [
        f"source/vendor/radxa/{release}/{edk2_base}",
        f"source/port/radxa/{release}/{edk2_base}",
    ]
    for ref in candidates:
        if ref_exists(repo, ref):
            return ref
    raise ReconstructionError(
        f"no Radxa source ref recorded for Radxa {release} on {edk2_base}; "
        f"expected one of: {', '.join(candidates)}"
    )


def radxa_target_ref(repo: Path, release: str, edk2_base: str, source: str) -> tuple[str, str]:
    if source not in {"auto", "vendor", "port"}:
        raise ReconstructionError("RADXA_SOURCE must be auto, vendor, or port")
    vendor_ref = f"source/vendor/radxa/{release}/{edk2_base}"
    port_ref = f"source/port/radxa/{release}/{edk2_base}"
    if source == "vendor":
        return vendor_ref, "vendor-source"
    if source == "port":
        return port_ref, "ported-vendor-source"
    if ref_exists(repo, vendor_ref) or not radxa_vendor_refs(repo, release):
        return vendor_ref, "vendor-source"
    return port_ref, "ported-vendor-source"


def compact_record_for_manifest(data: dict, record: dict) -> dict:
    compact = dict(record)
    defaults = data.get("defaults", {})
    if isinstance(defaults, dict):
        for key, value in defaults.items():
            if compact.get(key) == value:
                compact.pop(key)
    if RADXA_SOURCE_RE.match(str(compact.get("ref", ""))):
        for key in ("base_ref", "edk2_base", "format", "ported_from", "radxa_release", "type", "vendor"):
            compact.pop(key, None)
    return compact


def upsert_edk2_manifest(repo: Path, path: Path, target: str, record: dict) -> None:
    data = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {
        "defaults": {"immutable": True, "type": "base"},
        "component_templates": {
            "edk2": {
                "ref": "source/base/edk2/{edk2_ref}",
                "upstream_ref": "refs/tags/{edk2_ref}",
            },
            "edk2-platforms": {
                "ref": "source/base/edk2-platforms/{edk2_ref}",
                "upstream_ref": "refs/heads/master",
            },
            "edk2-non-osi": {
                "ref": "source/base/edk2-non-osi/{edk2_ref}",
                "upstream_ref": "refs/heads/master",
            },
        },
        "releases": [],
    }
    component = str(record.get("component", ""))
    edk2_ref = target.rsplit("/", 1)[-1]
    if component not in {"edk2", "edk2-platforms", "edk2-non-osi"}:
        raise ReconstructionError(f"cannot update EDK2 manifest for unknown component: {component}")

    releases = data.setdefault("releases", [])
    release_record = None
    for item in releases:
        if item.get("edk2_ref") == edk2_ref:
            release_record = item
            break
    if release_record is None:
        release_record = {"edk2_ref": edk2_ref, "components": {}}
        releases.append(release_record)
    components = release_record.setdefault("components", {})
    component_record = {
        "object_id": record["object_id"],
        "tree_id": record["tree_id"],
    }
    upstream_ref = record.get("upstream_ref")
    if component == "edk2":
        default_upstream_ref = f"refs/tags/{edk2_ref}"
    else:
        default_upstream_ref = "refs/heads/master"
        if record.get("selected_at_or_before"):
            release_record["selected_at_or_before"] = record["selected_at_or_before"]
    if upstream_ref and upstream_ref != default_upstream_ref:
        component_record["upstream_ref"] = upstream_ref
    components[component] = component_record
    releases.sort(key=lambda item: version_key(str(item.get("edk2_ref", ""))))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def upsert_manifest(repo: Path, target: str, record: dict) -> None:
    path = repo / manifest_path_for(target)
    if path.name == EDK2_REFS_MANIFEST:
        upsert_edk2_manifest(repo, path, target, record)
        return
    if path.exists():
        data = json.loads(path.read_text(encoding="utf-8"))
    else:
        data = {"refs": []}
    refs = data.setdefault("refs", [])
    refs[:] = [
        item
        for item in refs
        if item.get("ref") != target and target not in item.get("refs", [])
    ]
    refs.append(compact_record_for_manifest(data, record))
    refs.sort(key=lambda item: item.get("ref", ""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def manifest_record(repo: Path, target: str, **extra: str) -> dict:
    record = {
        "ref": target,
        "object_id": rev_parse(repo, target),
        "tree_id": tree_id(repo, target),
        "immutable": True,
    }
    record.update({k: v for k, v in extra.items() if v})
    return record


def operation_manifest_metadata(repo: Path, source: str, meta: dict[str, str]) -> dict[str, str]:
    record_meta = {key: value for key, value in meta.items() if not key.startswith("_")}
    if meta.get("type") == "vendor-source" and source and not source.startswith("<"):
        record_meta.setdefault("upstream_ref", rev_parse(repo, source))
    return record_meta


def materialise_vendor_ref(repo: Path, source_ref: str, release: str, verbose: bool) -> str:
    """Return a flat commit for a possibly submodule-shaped vendor source ref."""

    with temp_dir(repo, "vendor-materialise-") as tmp:
        worktree = Path(tmp) / "worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), resolve_ref(repo, source_ref), capture=not verbose)
        try:
            had_gitlinks = bool(gitlinks(worktree))
            materialise_submodules(repo, worktree, f"vendor-radxa-{release}", verbose)
            root_gitmodules = worktree / ".gitmodules"
            if root_gitmodules.exists():
                git(worktree, "rm", "-f", ".gitmodules", capture=not verbose)
            status = git(worktree, "status", "--porcelain").stdout.strip()
            if not status:
                return source_ref
            if verbose:
                note = " after flattening gitlinks" if had_gitlinks else " after removing root .gitmodules"
                print(f"committing materialised Radxa source {source_ref}{note}", file=sys.stderr)
            git(
                worktree,
                "commit",
                "-m",
                f"materialise: Radxa {release} vendor source from {source_ref}",
                capture=not verbose,
            )
            return rev_parse(worktree, "HEAD")
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False, capture=True)


def commit_radxa_source_snapshot(
    repo: Path,
    source_ref: str,
    release: str,
    edk2_base: str,
    record_type: str,
) -> str:
    """Create a clean snapshot commit for a Radxa vendor or port source tree."""

    source_tree = tree_id(repo, source_ref)
    kind = "vendor source" if record_type == "vendor-source" else "ported vendor source"
    if record_type == "ported-vendor-source":
        base_ref = resolve_ref_or_generated_cache(repo, cache_base_ref(edk2_base))
        source_delta_paths = [
            line
            for line in git(repo, "diff", "--name-only", base_ref, source_ref).stdout.splitlines()
            if line
        ]
        source_tree, _result = normalise_source_tree(
            repo,
            tree=source_tree,
            label=f"radxa-{release}-{edk2_base}",
            verbose=False,
            paths=source_delta_paths,
        )
    message = (
        f"source: record Radxa {release} {kind} for {edk2_base}\n\n"
        f"Source-Base: {CACHE_BASE_EDK2_PREFIX}{edk2_base}\n"
        f"Source-Model: materialised Radxa {kind} tree\n"
    )
    return git(repo, "commit-tree", source_tree, "-m", message).stdout.strip()


def ported_radxa_source_snapshot(
    repo: Path,
    source_ref: str,
    release: str,
    from_edk2_base: str,
    to_edk2_base: str,
    verbose: bool,
) -> str:
    old_base_ref = cache_base_ref(from_edk2_base)
    new_base_ref = cache_base_ref(to_edk2_base)
    message = (
        f"source: record Radxa {release} ported vendor source for {normalise_edk2_base(to_edk2_base)}\n\n"
        f"Source-Base: {new_base_ref}\n"
        f"Source-Ported-From: {old_base_ref}\n"
        f"Source-Ported-Input: {source_ref}\n"
        "Source-Model: materialised Radxa ported vendor source tree\n"
    )
    return apply_source_delta_to_base(
        repo,
        old_base_ref=old_base_ref,
        source_ref=source_ref,
        new_base_ref=new_base_ref,
        message=message,
        label=f"radxa-{release}-{normalise_edk2_base(to_edk2_base)}",
        normalise_source=True,
        verbose=verbose,
    )


def validate(args: argparse.Namespace) -> list[str]:
    missing: list[str] = []
    if args.type_ not in {"upstream", "vendor"}:
        missing.append("TYPE=upstream|vendor")
    if args.type_ == "upstream" and args.component not in UPSTREAM_COMPONENTS:
        missing.append("COMPONENT=edk2|edk2-platforms|edk2-non-osi|tf-a|op-tee")
    if args.type_ == "vendor" and args.vendor not in VENDORS:
        missing.append("VENDOR=radxa|cix")
    if args.type_ == "upstream" and not (args.release or args.ref):
        missing.append("RELEASE=<release> or REF=<object-id-or-ref>")
    if args.type_ == "vendor" and args.vendor == "cix" and not args.release:
        missing.append("RELEASE=<cix-release>")
    if args.type_ == "vendor" and args.vendor == "cix" and args.component:
        if args.component not in {"tf-a", "op-tee"}:
            missing.append("COMPONENT=tf-a|op-tee")
        if not args.arm_base:
            missing.append("ARM_BASE=<arm-release>")
        if not args.ref:
            missing.append("REF=<ported-ref>")
    if args.type_ == "vendor" and args.vendor == "radxa" and not (args.release and args.edk2_base):
        missing.append("RELEASE=<radxa-release> EDK2_BASE=<edk2-release>")
    if args.type_ == "vendor" and args.vendor == "radxa" and args.radxa_source not in {"auto", "vendor", "port"}:
        missing.append("RADXA_SOURCE=auto|vendor|port")
    return missing


def main() -> None:
    args = parser().parse_args()
    missing = validate(args)
    if missing:
        print(HELP)
        print("missing or invalid required variable(s): " + ", ".join(missing), file=sys.stderr)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)
    allow_replace = truthy(args.allow_replace)
    check_immutable_refs(repo, allow_manifest_update=write)

    operations: list[tuple[str, str, str, dict[str, str]]] = []
    if args.type_ == "upstream":
        release = args.release or args.ref
        operations.append(upstream_operation(repo, args.component, release, args.ref, verbose))
    elif args.vendor == "cix" and args.component:
        cix_release = args.release.removeprefix("v")
        target = f"source/port/cix/{cix_release}/{args.component}/{args.arm_base}"
        base_ref = f"source/base/arm/{args.component}/{args.arm_base}"
        operations.append((
            "local",
            args.ref,
            target,
            {
                "type": "ported-vendor-component",
                "vendor": "cix",
                "component": args.component,
                "cix_release": cix_release,
                "arm_base": args.arm_base,
                "base_ref": base_ref,
                "source_ref": args.ref,
            },
        ))
    elif args.vendor == "cix":
        records = cix_component_records(repo, args.release)
        if not records:
            raise ReconstructionError(
                f"no CIX component records found for release {args.release} in config/{CIX_REFS_MANIFEST}"
            )
        for item in records:
            remote = cix_record_remote(repo, item)
            source = str(item.get("upstream_ref") or item.get("object_id") or "")
            target = str(item.get("ref", ""))
            if not source or not target:
                raise ReconstructionError(f"incomplete CIX component record in config/{CIX_REFS_MANIFEST}: {item}")
            operations.append((
                remote,
                source,
                target,
                {
                    "type": str(item.get("type", "vendor-component")),
                    "vendor": "cix",
                    "component": str(item.get("component", "")),
                    "file": str(item.get("file", "")),
                    "remote": str(item.get("remote", "")),
                    "vendor_path": str(item.get("vendor_path", "")),
                    "upstream_ref": source,
                },
            ))
    else:
        edk2_base = normalise_edk2_base(args.edk2_base)
        target, record_type = radxa_target_ref(repo, args.release, edk2_base, args.radxa_source)
        if args.ref:
            source = args.ref
        elif record_type == "ported-vendor-source" and args.from_edk2_base:
            source = radxa_source_ref_for_base(repo, args.release, args.from_edk2_base)
        else:
            source = "<materialised-vendor-ref>"
        base_ref = cache_base_ref(edk2_base)
        operations.append((
            "local",
            source,
            target,
            {
                "type": record_type,
                "vendor": "radxa",
                "radxa_release": args.release,
                "edk2_base": edk2_base,
                "base_ref": base_ref,
                "format": "materialised source tree",
                "_from_edk2_base": normalise_edk2_base(args.from_edk2_base) if args.from_edk2_base else "",
                "_from_base_ref": cache_base_ref(args.from_edk2_base) if args.from_edk2_base else "",
            },
        ))

    existing_targets: dict[str, dict] = {}
    if not allow_replace:
        for _remote, source, target, meta in operations:
            state = existing_immutable_target(repo, target, source, meta)
            if state:
                existing_targets[target] = state

    if not write:
        print("dry run; set WRITE=1 to apply changes")
        for remote, source, target, meta in operations:
            if meta.get("_from_base_ref"):
                print(f"  port delta from {source} using {meta['_from_base_ref']} -> {meta['base_ref']}")
            if target in existing_targets:
                print_existing_target(existing_targets[target], dry_run=True)
            else:
                print(f"  {remote} {source} -> {target}")
        return

    integrated = 0
    localised = 0
    unchanged = 0
    for remote, source, target, meta in operations:
        if target in existing_targets:
            state = existing_targets[target]
            if materialise_existing_target_local_head(repo, state, verbose=verbose):
                localised += 1
            else:
                print_existing_target(state, dry_run=False)
                unchanged += 1
            continue
        record_meta = operation_manifest_metadata(repo, source, meta)
        if remote == "local":
            if source.startswith("<"):
                raise ReconstructionError("Radxa vendor/port integration requires REF=<vendor-ref-or-object> in WRITE mode")
            if meta.get("type") in {"vendor-source", "ported-vendor-source"}:
                base_ref = meta["base_ref"]
                resolve_ref_or_generated_cache(repo, base_ref)
                if not ref_exists(repo, source):
                    raise ReconstructionError(f"Radxa source ref is unavailable locally: {source}")
                generated_port = meta.get("type") == "ported-vendor-source" and meta.get("_from_edk2_base") and not args.ref
                if generated_port:
                    materialised_source = ported_radxa_source_snapshot(
                        repo,
                        source,
                        args.release,
                        str(meta["_from_edk2_base"]),
                        str(meta["edk2_base"]),
                        verbose,
                    )
                else:
                    materialised_source = source
                if truthy(args.materialise) and not generated_port:
                    materialised_source = materialise_vendor_ref(repo, source, args.release, verbose)
                if ref_exists(repo, target) and not allow_replace:
                    raise ReconstructionError(f"target immutable ref already exists: {target}")
                snapshot = commit_radxa_source_snapshot(
                    repo,
                    materialised_source,
                    args.release,
                    str(meta["edk2_base"]),
                    str(meta["type"]),
                )
                if allow_replace and ref_exists(repo, target):
                    git(repo, "branch", "-f", target, snapshot, capture=not verbose)
                else:
                    git(repo, "branch", target, snapshot, capture=not verbose)
            else:
                base_ref = meta.get("base_ref")
                if base_ref and not ref_exists(repo, base_ref):
                    raise ReconstructionError(f"CIX component uplift base ref is unavailable locally: {base_ref}")
                if ref_exists(repo, target) and not allow_replace:
                    raise ReconstructionError(f"target immutable ref already exists: {target}")
                if allow_replace:
                    git(repo, "branch", "-f", target, source, capture=not verbose)
                else:
                    git(repo, "branch", target, source, capture=not verbose)
        else:
            fetch_to_ref(repo, remote, source, target, verbose, allow_replace)
        clear_metadata_caches()
        upsert_manifest(repo, target, manifest_record(repo, target, **record_meta))
        integrated += 1
    if integrated:
        print("integration refs and config metadata updated")
    if localised:
        print(f"verified local immutable branches created: {localised}")
    if unchanged:
        print(f"requested integration refs already present and unchanged: {unchanged}")


if __name__ == "__main__":
    main_wrapper(main)
