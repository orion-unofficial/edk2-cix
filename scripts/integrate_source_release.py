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
    RADXA_SOURCE_RE,
    ReconstructionError,
    check_immutable_refs,
    git,
    load_json,
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


HELP = """integrate-source-release

Supported forms:
  make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202602 WRITE=1
  make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7 WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2 WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2 COMPONENT=tf-a ARM_BASE=v2.7 REF=<ported-ref> WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 REF=<vendor-ref> WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202602 REF=<ported-ref> RADXA_SOURCE=port WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1+<commit> EDK2_BASE=edk2-stable202208 REF=main WRITE=1

Required variables:
  TYPE=upstream|vendor
  COMPONENT=edk2|edk2-platforms|edk2-non-osi|tf-a|op-tee when TYPE=upstream
  VENDOR=radxa|cix when TYPE=vendor

Optional variables:
  RELEASE=<release-or-ref>  Release tag/version to integrate.
  EDK2_BASE=<release>       EDK2 base marker for Radxa source refs.
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
If the requested object is already present locally, WRITE=1 uses it without
contacting the external upstream/vendor remote.
Radxa non-release updates should use a descriptive RELEASE such as
1.2.1+<short-commit>. REF may point at a legacy submodule-shaped branch such
as main; with MATERIALISE=1, the source is flattened before the Radxa source ref
is recorded.
CIX TF-A/OP-TEE uplift experiments should use COMPONENT=tf-a|op-tee,
ARM_BASE=<arm-release>, and REF=<ported-ref>. This records the finished
component ref under source/component/cix/<cix-release>/<component>/<arm-base>.
"""

UPSTREAM_COMPONENTS = {"edk2", "edk2-platforms", "edk2-non-osi", "tf-a", "op-tee"}
VENDORS = {"radxa", "cix"}


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--type", dest="type_", default=os.environ.get("TYPE", ""))
    p.add_argument("--component", default=os.environ.get("COMPONENT", ""))
    p.add_argument("--vendor", default=os.environ.get("VENDOR", ""))
    p.add_argument("--release", default=os.environ.get("RELEASE", ""))
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
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


def remote_url(repo: Path, *, component: str = "", vendor: str = "", remote_type: str = "") -> str:
    remotes = load_json(repo, "config/remotes.json").get("remotes", {})
    for record in remotes.values():
        if component and record.get("component") != component:
            continue
        if vendor and record.get("vendor") != vendor:
            continue
        if remote_type and record.get("type") != remote_type:
            continue
        url = record.get("url")
        if url:
            return url
    detail = component or vendor or remote_type
    raise ReconstructionError(f"no remote URL recorded in config/remotes.json for {detail}")


def cix_component_records(repo: Path, release: str) -> list[dict[str, str]]:
    release = release.removeprefix("v")
    records = []
    for record in load_json(repo, "config/refs/cix.json").get("refs", []):
        ref = str(record.get("ref", ""))
        if not ref.startswith(f"source/component/cix/{release}/"):
            continue
        if record.get("type") not in {"vendor-bundle", "vendor-component"}:
            continue
        records.append(record)
    return sorted(records, key=lambda item: str(item.get("ref", "")))


def cix_record_remote(repo: Path, record: dict[str, str]) -> str:
    record_type = str(record.get("type", ""))
    if record_type == "vendor-bundle":
        return remote_url(repo, vendor="cix", remote_type="vendor-bundle")
    return remote_url(repo, component=str(record.get("component", "")), vendor="cix")


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
        return "config/refs/edk2.json"
    if target.startswith("source/base/arm/"):
        return "config/refs/arm.json"
    if target.startswith("source/component/cix/"):
        return "config/refs/cix.json"
    if target.startswith(("source/vendor/radxa/", "source/port/radxa/")):
        return "config/refs/radxa.json"
    return "config/refs/integrated.json"


def radxa_vendor_refs(repo: Path, release: str) -> list[str]:
    result = git(repo, "for-each-ref", "--format=%(refname:short)", f"refs/heads/source/vendor/radxa/{release}", check=False)
    if result.returncode != 0:
        return []
    return [line for line in result.stdout.splitlines() if line]


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
    if path.name == "edk2.json":
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
    message = (
        f"source: record Radxa {release} {kind} for {edk2_base}\n\n"
        f"Source-Base: {CACHE_BASE_EDK2_PREFIX}{edk2_base}\n"
        f"Source-Model: materialised Radxa {kind} tree\n"
    )
    return git(repo, "commit-tree", source_tree, "-m", message).stdout.strip()


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
        target = upstream_target(args.component, release)
        source = args.ref or f"refs/tags/{release}"
        remote = remote_url(repo, component=args.component, remote_type="upstream")
        operations.append((remote, source, target, {"type": "base", "component": args.component, "upstream_ref": source}))
    elif args.vendor == "cix" and args.component:
        cix_release = args.release.removeprefix("v")
        target = f"source/component/cix/{cix_release}/{args.component}/{args.arm_base}"
        base_ref = f"source/base/arm/{args.component}/{args.arm_base}"
        operations.append((
            "local",
            args.ref,
            target,
            {
                "type": "vendor-component-uplift",
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
                f"no CIX component records found for release {args.release} in config/refs/cix.json"
            )
        for item in records:
            remote = cix_record_remote(repo, item)
            source = str(item.get("upstream_ref") or item.get("object_id") or "")
            target = str(item.get("ref", ""))
            if not source or not target:
                raise ReconstructionError(f"incomplete CIX component record in config/refs/cix.json: {item}")
            operations.append((
                remote,
                source,
                target,
                {
                    "type": str(item.get("type", "vendor-component")),
                    "vendor": "cix",
                    "component": str(item.get("component", "")),
                    "vendor_path": str(item.get("vendor_path", "")),
                    "upstream_ref": source,
                },
            ))
    else:
        target, record_type = radxa_target_ref(repo, args.release, args.edk2_base, args.radxa_source)
        source = args.ref or "<materialised-vendor-ref>"
        base_ref = f"{CACHE_BASE_EDK2_PREFIX}{args.edk2_base}"
        operations.append((
            "local",
            source,
            target,
            {
                "type": record_type,
                "vendor": "radxa",
                "radxa_release": args.release,
                "edk2_base": args.edk2_base,
                "base_ref": base_ref,
                "format": "materialised source tree",
            },
        ))

    if not write:
        print("dry run; set WRITE=1 to create refs")
        for remote, source, target, _meta in operations:
            print(f"  {remote} {source} -> {target}")
        return

    for remote, source, target, meta in operations:
        if remote == "local":
            if source.startswith("<"):
                raise ReconstructionError("Radxa vendor/port integration requires REF=<vendor-ref-or-object> in WRITE mode")
            if meta.get("type") in {"vendor-source", "ported-vendor-source"}:
                base_ref = meta["base_ref"]
                resolve_ref_or_generated_cache(repo, base_ref)
                if not ref_exists(repo, source):
                    raise ReconstructionError(f"Radxa source ref is unavailable locally: {source}")
                materialised_source = source
                if truthy(args.materialise):
                    materialised_source = materialise_vendor_ref(repo, source, args.release, verbose)
                if ref_exists(repo, target) and not allow_replace:
                    raise ReconstructionError(f"target immutable ref already exists: {target}")
                snapshot = commit_radxa_source_snapshot(
                    repo,
                    materialised_source,
                    args.release,
                    args.edk2_base,
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
        upsert_manifest(repo, target, manifest_record(repo, target, **meta))
    print("integration refs and config/refs metadata updated")


if __name__ == "__main__":
    main_wrapper(main)
