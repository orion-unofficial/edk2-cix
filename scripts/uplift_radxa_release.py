#!/usr/bin/env python3
"""Carry an unofficial firmware line onto a newer Radxa release."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

from import_workflow import (
    ZERO_OID,
    ensure_target_not_checked_out_dirty,
    ref_oid,
    transaction_update_refs,
)
from reconstruction_common import (
    BUILD_INFRA_OVERLAY_PATHS,
    UNOFFICIAL_REFS_MANIFEST,
    ReconstructionError,
    branch_to_ref,
    cache_dir,
    check_immutable_refs,
    clear_metadata_caches,
    commit_tree_with_files,
    format_duration,
    git,
    load_json,
    load_ref_records,
    main_wrapper,
    normalise_unofficial_immutability_policy,
    radxa_source_ref,
    ref_exists,
    release_entry,
    release_for_edk2_ref,
    repo_root,
    rev_parse,
    safe_name,
    selected_unofficial_line_policy,
    tree_id,
    truthy,
    unofficial_line_policies,
    unofficial_source_policy,
    update_ref_record,
    version_key,
    write_json,
)
from source_policy import enforce_source_tree_policy
from source_porting import (
    apply_source_delta_to_base,
    normalise_overlay_tree,
    normalise_source_tree,
    overlay_paths_from_source,
    preserve_conflict_worktree,
)


HELP = """uplift-radxa-release

Carries a project-maintained firmware line from one Radxa release to the next:

  1. replay the adjacent raw Radxa vendor delta onto the previous EDK2 port
  2. record source/port/radxa/<TO_RELEASE>/<EDK2_BASE>
  3. replay the previous unofficial delta onto that new port
  4. record source/unofficial/<TO_RELEASE>/<EDK2_BASE>
  5. move source/unofficial/<LINE>/current and update line policy
  6. render the previous exact checkpoint and resulting custom source target
  7. verify the complete build matrix

Required variables:
  FROM_RELEASE=<release>  Previous Radxa release.
  TO_RELEASE=<release>    New Radxa release.

Optional variables:
  LINE=<major.minor>
      Unofficial development line to update. Default: TO_RELEASE major.minor.
  EDK2_BASE=<edk2-stableYYYYMM>
      EDK2 base for the maintained line. Default: selected line policy, then
      the default line policy.
  CIX_RELEASE=<release>
      CIX component set for the rendered source target. Default: line policy.
  FROM_UNOFFICIAL_REF=<ref>
      Previous unofficial source. When supplied, this deliberately overrides
      an exact FROM_RELEASE checkpoint; use it to seed a new line from a
      reviewed mutable line tip or when no checkpoint/current policy exists.
  PORT_REF=<ref>
      Manually resolved new Radxa port from a conflict handoff.
  UNOFFICIAL_REF=<ref>
      Manually resolved unofficial tree from a conflict handoff.
  UNOFFICIAL_REF_STAGE=auto|source|overlay|final
      Override resume-stage detection for a reviewed unofficial commit.
      Default: auto.
  MAKE_DEFAULT=0|1
      Select LINE as the default build line. Existing default is preserved
      unless this is 1 or no default has been configured.
  SKIP_RENDER=0|1
      Skip rendered source-target refresh. Default: 0.
  VERIFY=0|1
      Run verify-build-matrix after rendering. Default: 1.
  WRITE=0|1
      Required before refs or configuration are changed.
  ALLOW_REPLACE=0|1
      Permit replacement of an existing exact port/checkpoint. Default: 0.
  V=0|1
      Print delegated commands and git operations.

On a genuine source conflict, the common porting primitive preserves a
worktree and reports the conflicted paths. Resolve and commit there, then rerun
with PORT_REF=<commit> or UNOFFICIAL_REF=<commit>. Completed stages are
idempotent and are skipped.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=HELP,
    )
    p.add_argument("--from-release", default=os.environ.get("FROM_RELEASE", ""))
    p.add_argument("--to-release", default=os.environ.get("TO_RELEASE", ""))
    p.add_argument("--line", default=os.environ.get("LINE", ""))
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
    p.add_argument("--cix-release", default=os.environ.get("CIX_RELEASE", ""))
    p.add_argument("--from-unofficial-ref", default=os.environ.get("FROM_UNOFFICIAL_REF", ""))
    p.add_argument("--port-ref", default=os.environ.get("PORT_REF", ""))
    p.add_argument("--unofficial-ref", default=os.environ.get("UNOFFICIAL_REF", ""))
    p.add_argument(
        "--unofficial-ref-stage",
        default=os.environ.get("UNOFFICIAL_REF_STAGE", "auto"),
    )
    p.add_argument("--make-default", default=os.environ.get("MAKE_DEFAULT", "0"))
    p.add_argument("--skip-render", default=os.environ.get("SKIP_RENDER", "0"))
    p.add_argument("--verify", default=os.environ.get("VERIFY", "1"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--allow-replace", default=os.environ.get("ALLOW_REPLACE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def normalise_edk2_base(value: str) -> str:
    value = value.strip()
    if not value:
        return ""
    if value.startswith("edk2-stable"):
        return value
    if value.startswith("edk2-"):
        return "edk2-stable" + value.removeprefix("edk2-")
    return "edk2-stable" + value


def release_line(release: str) -> str:
    fields = release.split(".")
    if len(fields) < 2 or not all(field.isdigit() for field in fields[:2]):
        raise ReconstructionError(
            f"cannot derive an unofficial line from Radxa release {release!r}; pass LINE=<major.minor>"
        )
    return ".".join(fields[:2])


def line_policy(
    policy: dict[str, Any],
    line: str,
) -> tuple[str, dict[str, Any]]:
    default_line, lines = unofficial_line_policies(policy)
    selected = lines.get(line)
    if selected is None and default_line:
        selected = lines.get(default_line)
    return default_line, dict(selected or {})


def resolve_from_unofficial_ref(
    repo: Path,
    *,
    from_release: str,
    edk2_base: str,
    line: str,
    explicit_ref: str,
    policy: dict[str, Any],
) -> str:
    exact = f"source/unofficial/{from_release}/{edk2_base}"
    if explicit_ref:
        if not ref_exists(repo, explicit_ref):
            raise ReconstructionError(f"FROM_UNOFFICIAL_REF is unavailable locally: {explicit_ref}")
        return explicit_ref
    if ref_exists(repo, exact):
        return exact

    _default_line, lines = unofficial_line_policies(policy)
    selected = lines.get(line, {})
    candidate = str(selected.get("current_ref", "")).strip()
    recorded_release = str(selected.get("current_radxa_release", "")).strip()
    if candidate and recorded_release == from_release and ref_exists(repo, candidate):
        return candidate

    raise ReconstructionError(
        f"no unofficial source checkpoint exists for {from_release} on {edk2_base}; "
        "pass FROM_UNOFFICIAL_REF=<reviewed-source-ref> to initialise this line"
    )


def source_target(edk2_base: str, cix_release: str, radxa_release: str) -> str:
    release = release_for_edk2_ref(edk2_base)
    if cix_release:
        return f"edk2-{release}/cix-{cix_release}/radxa-{radxa_release}/unofficial"
    return f"edk2-{release}/radxa-{radxa_release}/unofficial"


def rendered_target_is_current(repo: Path, target: str) -> bool:
    branch, entry = release_entry(repo, target, require=True)
    expected_tree = str(entry.get("tree_id", "")).strip()
    return bool(
        expected_tree
        and ref_exists(repo, branch)
        and tree_id(repo, branch) == expected_tree
    )


def run_script(
    repo: Path,
    script: str,
    env_updates: dict[str, str],
    *,
    verbose: bool,
) -> None:
    env = os.environ.copy()
    env.update(env_updates)
    env["PYTHONPATH"] = str(repo / "scripts")
    command = [sys.executable, str(repo / "scripts" / script), "--v", env.get("V", "0")]
    if verbose:
        assignments = " ".join(
            f"{key}={value}" for key, value in sorted(env_updates.items()) if value
        )
        print(f"[radxa-uplift] {assignments} {' '.join(command)}")
    result = subprocess.run(command, cwd=repo, env=env, check=False)
    if result.returncode != 0:
        raise SystemExit(result.returncode)
    clear_metadata_caches()


def run_make(repo: Path, target: str, *, verbose: bool) -> None:
    command = ["make", "--no-print-directory", target]
    if verbose:
        print(f"[radxa-uplift] {' '.join(command)}")
    result = subprocess.run(command, cwd=repo, check=False)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def port_candidate(
    repo: Path,
    *,
    from_release: str,
    to_release: str,
    edk2_base: str,
    resolved_ref: str,
    verbose: bool,
) -> str:
    old_port = radxa_source_ref(repo, from_release, edk2_base)
    if resolved_ref:
        if not ref_exists(repo, resolved_ref):
            raise ReconstructionError(f"PORT_REF is unavailable locally: {resolved_ref}")
        resolved = rev_parse(repo, resolved_ref)
        tree, _result = normalise_source_tree(
            repo,
            tree=tree_id(repo, resolved),
            label=f"radxa-{from_release}-to-{to_release}-{edk2_base}",
            verbose=verbose,
            paths=changed_paths(repo, old_port, resolved),
        )
        message = git(repo, "show", "-s", "--format=%B", resolved).stdout
        return git(repo, "commit-tree", tree, "-m", message).stdout.strip()

    old_vendor = radxa_source_ref(repo, from_release, "edk2-stable202208")
    new_vendor = radxa_source_ref(repo, to_release, "edk2-stable202208")
    message = (
        f"source: port Radxa {to_release} vendor delta to {edk2_base}\n\n"
        f"Source-Vendor-From: {old_vendor}\n"
        f"Source-Vendor-To: {new_vendor}\n"
        f"Source-Ported-Onto: {old_port}\n"
    )
    return apply_source_delta_to_base(
        repo,
        old_base_ref=old_vendor,
        source_ref=new_vendor,
        new_base_ref=old_port,
        message=message,
        label=f"radxa-{from_release}-to-{to_release}-{edk2_base}",
        normalise_source=True,
        resume_variable="PORT_REF",
        verbose=verbose,
    )


def unofficial_candidate(
    repo: Path,
    *,
    from_release: str,
    to_release: str,
    edk2_base: str,
    from_unofficial_ref: str,
    new_port_ref: str,
    resolved_ref: str,
    verbose: bool,
    resolved_stage: str = "auto",
) -> str:
    message = (
        f"source: carry unofficial firmware from Radxa {from_release} to {to_release}\n\n"
        f"Source-Unofficial-From: {from_unofficial_ref}\n"
        f"Source-Port-From: {radxa_source_ref(repo, from_release, edk2_base)}\n"
        f"Source-Port-To: {new_port_ref}\n"
    )
    if resolved_ref:
        if not ref_exists(repo, resolved_ref):
            raise ReconstructionError(f"UNOFFICIAL_REF is unavailable locally: {resolved_ref}")
        resolved = rev_parse(repo, resolved_ref)
        label = f"unofficial-{from_release}-to-{to_release}-{edk2_base}"
        stage = resolved_unofficial_stage(repo, resolved, resolved_stage)
        if stage == "final":
            return resolved
        tree = tree_id(repo, resolved)
        if stage == "source":
            tree, conflicts, merge_detail = normalise_overlay_tree(
                repo,
                tree=tree,
                source_ref=from_unofficial_ref,
                label=label,
                verbose=verbose,
            )
            if conflicts:
                worktree = preserve_conflict_worktree(
                    repo,
                    tree=tree,
                    label=label,
                    merge_output=merge_detail,
                    source_ref=from_unofficial_ref,
                    new_base_ref=new_port_ref,
                    resume_variable="UNOFFICIAL_REF",
                    conflict_paths=conflicts,
                    conflict_stage="overlay",
                    verbose=verbose,
                )
                raise ReconstructionError(
                    "could not rebase custom overlays onto the new source tree"
                    f"\n\nsource-port conflict worktree preserved at: {worktree}\n"
                    "Resolve conflicts there, commit the result, and rerun with "
                    "UNOFFICIAL_REF=<resolved-commit>."
                )
        tree, _result = normalise_source_tree(
            repo,
            tree=tree,
            label=label,
            verbose=verbose,
            paths=changed_paths(repo, new_port_ref, resolved),
        )
        return git(repo, "commit-tree", tree, "-m", message).stdout.strip()

    old_port = radxa_source_ref(repo, from_release, edk2_base)
    return apply_source_delta_to_base(
        repo,
        old_base_ref=old_port,
        source_ref=from_unofficial_ref,
        new_base_ref=new_port_ref,
        message=message,
        label=f"unofficial-{from_release}-to-{to_release}-{edk2_base}",
        source_owned_paths=BUILD_INFRA_OVERLAY_PATHS,
        normalise_source=True,
        resume_variable="UNOFFICIAL_REF",
        verbose=verbose,
    )


def align_release_metadata(
    repo: Path,
    *,
    candidate: str,
    new_port_ref: str,
    to_release: str,
    verbose: bool,
) -> str:
    """Take release identity from the new Radxa port, not the old build overlay."""

    label = f"radxa-{to_release}-release-metadata"
    tree = overlay_paths_from_source(
        repo,
        tree=tree_id(repo, candidate),
        source_ref=new_port_ref,
        paths=("debian/changelog",),
        label=label,
        verbose=verbose,
    )
    version_ref = commit_tree_with_files(
        repo,
        {"VERSION": f"{to_release}\n".encode("utf-8")},
        f"source: stage Radxa {to_release} release metadata",
    )
    tree = overlay_paths_from_source(
        repo,
        tree=tree,
        source_ref=version_ref,
        paths=("VERSION",),
        label=label,
        verbose=verbose,
    )
    if tree == tree_id(repo, candidate):
        return candidate
    message = git(repo, "show", "-s", "--format=%B", candidate).stdout.rstrip()
    return git(
        repo,
        "commit-tree",
        tree,
        "-m",
        f"{message}\n\nSource-Release-Metadata: Radxa {to_release}",
    ).stdout.strip()


def resolved_unofficial_stage(repo: Path, resolved: str, requested: str) -> str:
    stage = requested.strip().lower() or "auto"
    allowed = {"auto", "source", "overlay", "final"}
    if stage not in allowed:
        raise ReconstructionError(
            "UNOFFICIAL_REF_STAGE must be one of: " + ", ".join(sorted(allowed))
        )
    if stage != "auto":
        return stage

    parent = git(repo, "rev-parse", f"{resolved}^", check=False)
    if parent.returncode != 0:
        return "source"
    parent_oid = parent.stdout.strip()
    message = git(repo, "show", "-s", "--format=%B", parent_oid).stdout
    prefix = "Source-Port-Conflict-Stage:"
    for line in message.splitlines():
        if line.startswith(prefix):
            recorded = line[len(prefix) :].strip().lower()
            if recorded in {"source", "overlay"}:
                return recorded

    markers = git(
        repo,
        "grep",
        "-I",
        "-l",
        "-e",
        "^<<<<<<< ",
        parent_oid,
        "--",
        check=False,
    )
    marker_paths = [
        line.split(":", 1)[1]
        for line in markers.stdout.splitlines()
        if ":" in line
    ]
    if marker_paths and all(path.startswith("custom/overlay/") for path in marker_paths):
        return "overlay"
    return "source"


def changed_paths(repo: Path, old_ref: str, new_ref: str) -> list[str]:
    result = git(repo, "diff", "--name-only", old_ref, new_ref)
    return sorted(line for line in result.stdout.splitlines() if line)


def write_uplift_report(
    repo: Path,
    *,
    from_release: str,
    to_release: str,
    edk2_base: str,
    old_vendor: str,
    new_vendor: str,
    old_port: str,
    from_unofficial_ref: str,
) -> Path:
    vendor_paths = changed_paths(repo, old_vendor, new_vendor)
    unofficial_paths = changed_paths(repo, old_port, from_unofficial_ref)
    overlap = sorted(set(vendor_paths) & set(unofficial_paths))
    owned_overlap = [
        path
        for path in overlap
        if any(
            path == root or path.startswith(root.rstrip("/") + "/")
            for root in BUILD_INFRA_OVERLAY_PATHS
        )
    ]
    review_overlap = sorted(set(overlap) - set(owned_overlap))
    report = {
        "edk2_base": edk2_base,
        "from_release": from_release,
        "from_unofficial_ref": from_unofficial_ref,
        "old_port": old_port,
        "old_vendor": old_vendor,
        "policy_owned_overlap_paths": owned_overlap,
        "review_overlap_paths": review_overlap,
        "to_release": to_release,
        "unofficial_delta_path_count": len(unofficial_paths),
        "vendor_delta_paths": vendor_paths,
    }
    name = safe_name(f"{from_release}-to-{to_release}-{edk2_base}") + ".json"
    path = cache_dir(repo, "reports", "radxa-uplift") / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        "[radxa-uplift] overlap report: "
        f"{path} ({len(owned_overlap)} policy-owned, {len(review_overlap)} require merge review)"
    )
    return path


def checkpoint_record(
    repo: Path,
    *,
    ref: str,
    source_oid: str,
    line: str,
    radxa_release: str,
    edk2_base: str,
    radxa_ref: str,
    previous_ref: str,
) -> None:
    update_ref_record(
        repo,
        UNOFFICIAL_REFS_MANIFEST,
        ref,
        {
            "edk2_base": edk2_base,
            "immutable": True,
            "line": line,
            "object_id": source_oid,
            "previous_unofficial_ref": previous_ref,
            "radxa_release": radxa_release,
            "radxa_source_ref": radxa_ref,
            "tree_id": tree_id(repo, source_oid),
            "type": "unofficial-release-checkpoint",
        },
    )


def checkpoint_manifest_record(repo: Path, ref: str) -> dict[str, Any] | None:
    for record in load_ref_records(repo):
        if (
            record.get("manifest") == f"config/{UNOFFICIAL_REFS_MANIFEST}"
            and record.get("ref") == ref
        ):
            return record
    return None


def checkpoint_predecessor(
    repo: Path,
    *,
    ref: str,
    line: str,
    radxa_release: str,
    edk2_base: str,
    preferred_ref: str,
) -> str:
    candidates: list[tuple[tuple[Any, ...], str]] = []
    for record in load_ref_records(repo):
        candidate_ref = str(record.get("ref", ""))
        candidate_release = str(record.get("radxa_release", ""))
        if (
            record.get("manifest") != f"config/{UNOFFICIAL_REFS_MANIFEST}"
            or record.get("type") != "unofficial-release-checkpoint"
            or record.get("line") != line
            or record.get("edk2_base") != edk2_base
            or not candidate_ref
            or not candidate_release
            or version_key(candidate_release) >= version_key(radxa_release)
        ):
            continue
        candidates.append((version_key(candidate_release), candidate_ref))
    if candidates:
        return max(candidates)[1]
    if preferred_ref != ref:
        return preferred_ref

    raise ReconstructionError(
        f"cannot determine predecessor provenance for {ref}; "
        "pass FROM_UNOFFICIAL_REF=<reviewed-source-ref>"
    )


def ensure_checkpoint_record(
    repo: Path,
    *,
    ref: str,
    source_oid: str,
    line: str,
    radxa_release: str,
    edk2_base: str,
    radxa_ref: str,
    preferred_previous_ref: str,
) -> None:
    record = checkpoint_manifest_record(repo, ref)
    if (
        record is not None
        and record.get("previous_unofficial_ref") != ref
        and record.get("line") == line
    ):
        if (
            record.get("object_id") == source_oid
            and record.get("tree_id") == tree_id(repo, source_oid)
        ):
            return
        previous_ref = str(record["previous_unofficial_ref"])
    else:
        previous_ref = checkpoint_predecessor(
            repo,
            ref=ref,
            line=line,
            radxa_release=radxa_release,
            edk2_base=edk2_base,
            preferred_ref=preferred_previous_ref,
        )
    checkpoint_record(
        repo,
        ref=ref,
        source_oid=source_oid,
        line=line,
        radxa_release=radxa_release,
        edk2_base=edk2_base,
        radxa_ref=radxa_ref,
        previous_ref=previous_ref,
    )


def update_policy(
    repo: Path,
    *,
    line: str,
    line_ref: str,
    radxa_release: str,
    edk2_base: str,
    cix_release: str,
    make_default: bool,
) -> None:
    path = repo / "config" / "policies.json"
    data = load_json(repo, "config/policies.json")
    old_policy = data.get("unofficial_source_policy", {})
    if not isinstance(old_policy, dict):
        raise ReconstructionError("config/policies.json unofficial_source_policy must be an object")

    default_line, lines = unofficial_line_policies(old_policy)
    migrated_lines = {name: dict(record) for name, record in lines.items()}
    migrated_lines[line] = {
        "current_cix_release": cix_release,
        "current_edk2_release": release_for_edk2_ref(edk2_base),
        "current_radxa_release": radxa_release,
        "current_ref": line_ref,
    }
    new_default = line if make_default or not default_line else default_line
    data["unofficial_source_policy"] = {
        "default_line": new_default,
        "lines": dict(sorted(migrated_lines.items(), key=lambda item: version_key(item[0]))),
        "prefer_versioned_default_alias": truthy(
            old_policy.get("prefer_versioned_default_alias", True)
        ),
    }
    normalise_unofficial_immutability_policy(data)
    write_json(path, data)


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)
    allow_replace = truthy(args.allow_replace)

    if not args.from_release or not args.to_release:
        print(HELP)
        raise SystemExit("missing required variables: FROM_RELEASE=<release> TO_RELEASE=<release>")
    if version_key(args.to_release) <= version_key(args.from_release):
        raise ReconstructionError("TO_RELEASE must sort after FROM_RELEASE")

    line = args.line.strip() or release_line(args.to_release)
    if release_line(args.to_release) != line:
        raise ReconstructionError(
            f"TO_RELEASE {args.to_release} belongs to line {release_line(args.to_release)}, not LINE={line}"
        )

    policy = unofficial_source_policy(repo)
    _default_line, selected = line_policy(policy, line)
    edk2_base = normalise_edk2_base(
        args.edk2_base or str(selected.get("current_edk2_release", ""))
    )
    if not edk2_base:
        _selected_line, default_selected = selected_unofficial_line_policy(policy)
        edk2_base = normalise_edk2_base(
            str(default_selected.get("current_edk2_release", ""))
        )
    if not edk2_base:
        raise ReconstructionError(
            "EDK2_BASE is required when no unofficial line policy supplies it"
        )
    cix_release = args.cix_release or str(selected.get("current_cix_release", ""))
    if not cix_release:
        _selected_line, default_selected = selected_unofficial_line_policy(policy)
        cix_release = str(default_selected.get("current_cix_release", ""))

    from_unofficial_ref = resolve_from_unofficial_ref(
        repo,
        from_release=args.from_release,
        edk2_base=edk2_base,
        line=line,
        explicit_ref=args.from_unofficial_ref,
        policy=policy,
    )
    old_port = radxa_source_ref(repo, args.from_release, edk2_base)
    old_vendor = radxa_source_ref(repo, args.from_release, "edk2-stable202208")
    new_vendor = radxa_source_ref(repo, args.to_release, "edk2-stable202208")
    target_port = f"source/port/radxa/{args.to_release}/{edk2_base}"
    from_checkpoint = f"source/unofficial/{args.from_release}/{edk2_base}"
    target_checkpoint = f"source/unofficial/{args.to_release}/{edk2_base}"
    line_ref = f"source/unofficial/{line}/current"

    print(f"[radxa-uplift] line: {line}")
    print(f"[radxa-uplift] Radxa release: {args.from_release} -> {args.to_release}")
    print(f"[radxa-uplift] EDK2 base: {edk2_base}")
    print(f"[radxa-uplift] previous unofficial source: {from_unofficial_ref}")
    print(f"[radxa-uplift] port target: {target_port}")
    print(f"[radxa-uplift] checkpoint target: {target_checkpoint}")
    print(f"[radxa-uplift] line tip: {line_ref}")
    write_uplift_report(
        repo,
        from_release=args.from_release,
        to_release=args.to_release,
        edk2_base=edk2_base,
        old_vendor=old_vendor,
        new_vendor=new_vendor,
        old_port=old_port,
        from_unofficial_ref=from_unofficial_ref,
    )

    check_immutable_refs(repo, allow_manifest_update=write)

    if ref_exists(repo, target_port) and not allow_replace:
        new_port = rev_parse(repo, target_port)
        print(f"[radxa-uplift] {target_port} already exists; skipping port stage")
    else:
        new_port = port_candidate(
            repo,
            from_release=args.from_release,
            to_release=args.to_release,
            edk2_base=edk2_base,
            resolved_ref=args.port_ref,
            verbose=verbose,
        )
        print(f"[radxa-uplift] port candidate: {new_port} (tree {tree_id(repo, new_port)})")
        if write:
            run_script(
                repo,
                "integrate_source_release.py",
                {
                    "TYPE": "vendor",
                    "VENDOR": "radxa",
                    "RELEASE": args.to_release,
                    "EDK2_BASE": edk2_base,
                    "RADXA_SOURCE": "port",
                    "REF": new_port,
                    "MATERIALISE": "0",
                    "WRITE": "1",
                    "ALLOW_REPLACE": "1" if allow_replace else "0",
                    "V": "1" if verbose else "0",
                },
                verbose=verbose,
            )
    if write:
        new_port = rev_parse(repo, target_port)
        update_ref_record(
            repo,
            "refs-radxa.json",
            target_port,
            {
                "ported_from": old_port,
                "vendor_delta_from": radxa_source_ref(
                    repo, args.from_release, "edk2-stable202208"
                ),
                "vendor_delta_to": new_vendor,
            },
        )
        clear_metadata_caches()

    if ref_exists(repo, target_checkpoint) and not allow_replace:
        candidate = rev_parse(repo, target_checkpoint)
        print(f"[radxa-uplift] {target_checkpoint} already exists; skipping unofficial stage")
    else:
        candidate = unofficial_candidate(
            repo,
            from_release=args.from_release,
            to_release=args.to_release,
            edk2_base=edk2_base,
            from_unofficial_ref=from_unofficial_ref,
            new_port_ref=target_port if write else new_port,
            resolved_ref=args.unofficial_ref,
            verbose=verbose,
            resolved_stage=args.unofficial_ref_stage,
        )
    candidate = align_release_metadata(
        repo,
        candidate=candidate,
        new_port_ref=target_port if write else new_port,
        to_release=args.to_release,
        verbose=verbose,
    )
    print(
        f"[radxa-uplift] unofficial candidate: {candidate} "
        f"(tree {tree_id(repo, candidate)})"
    )
    enforce_source_tree_policy(
        repo,
        ref=candidate,
        label=f"unofficial candidate for Radxa {args.to_release}",
    )

    updates: list[tuple[str, str, str]] = []
    create_from_checkpoint = not ref_exists(repo, from_checkpoint)
    if create_from_checkpoint:
        updates.append(
            (
                branch_to_ref(from_checkpoint),
                rev_parse(repo, from_unofficial_ref),
                ZERO_OID,
            )
        )
    target_old = ref_oid(repo, target_checkpoint) or ZERO_OID
    if target_old != candidate:
        if target_old != ZERO_OID and not allow_replace:
            raise ReconstructionError(
                f"target unofficial checkpoint already exists: {target_checkpoint}"
            )
        updates.append((branch_to_ref(target_checkpoint), candidate, target_old))
    line_old = ref_oid(repo, line_ref) or ZERO_OID
    if line_old != candidate:
        updates.append((branch_to_ref(line_ref), candidate, line_old))

    if not write:
        print("dry run; set WRITE=1 to update refs and policy")
        for full_ref, new_oid, old_oid in updates:
            print(f"  {full_ref}: {old_oid} -> {new_oid}")
        print(
            "  config/policies.json: "
            f"line {line} -> Radxa {args.to_release} on {edk2_base}"
        )
        return

    ensure_target_not_checked_out_dirty(repo, line_ref)
    if ref_exists(repo, target_checkpoint):
        ensure_target_not_checked_out_dirty(repo, target_checkpoint)
    transaction_update_refs(repo, updates)
    ensure_checkpoint_record(
        repo,
        ref=from_checkpoint,
        source_oid=rev_parse(repo, from_checkpoint),
        line=release_line(args.from_release),
        radxa_release=args.from_release,
        edk2_base=edk2_base,
        radxa_ref=old_port,
        preferred_previous_ref=from_unofficial_ref,
    )
    ensure_checkpoint_record(
        repo,
        ref=target_checkpoint,
        source_oid=rev_parse(repo, target_checkpoint),
        line=line,
        radxa_release=args.to_release,
        edk2_base=edk2_base,
        radxa_ref=target_port,
        preferred_previous_ref=from_checkpoint,
    )
    update_policy(
        repo,
        line=line,
        line_ref=line_ref,
        radxa_release=args.to_release,
        edk2_base=edk2_base,
        cix_release=cix_release,
        make_default=truthy(args.make_default),
    )
    clear_metadata_caches()

    render_targets = [
        source_target(edk2_base, cix_release, release)
        for release in (args.from_release, args.to_release)
    ]
    if truthy(args.skip_render):
        print("[radxa-uplift] SKIP_RENDER=1; not refreshing rendered source target")
    else:
        for render_target in render_targets:
            if rendered_target_is_current(repo, render_target):
                print(f"[radxa-uplift] {render_target} already matches its source checkpoint")
                continue
            print(f"[radxa-uplift] rendering {render_target}")
            run_script(
                repo,
                "render_release_branch.py",
                {
                    "RELEASE": render_target,
                    "PERSIST": "1",
                    "REBUILD": "1",
                    "FORCE": "1",
                    "V": "1" if verbose else "0",
                },
                verbose=verbose,
            )

    if truthy(args.verify):
        print("[radxa-uplift] verifying build matrix")
        run_make(repo, "verify-build-matrix", verbose=verbose)

    print(
        "Radxa release uplift completed in "
        f"{format_duration(time.monotonic() - started)}"
    )


if __name__ == "__main__":
    main_wrapper(main)
