#!/usr/bin/env python3
"""Check whether recorded upstream/vendor source refs lag external remotes."""

from __future__ import annotations

import argparse
import json
import os
import re
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from reconstruction_common import (
    ARM_REFS_MANIFEST,
    CIX_REFS_MANIFEST,
    EDK2_REFS_MANIFEST,
    RADXA_REFS_MANIFEST,
    ReconstructionError,
    available_radxa_releases,
    format_duration,
    latest_value,
    load_json,
    main_wrapper,
    matrix_release_values,
    ref_manifest_records,
    repo_root,
    run,
    show_file,
    truthy,
    version_key,
)


HELP = """check-upstream-versions

No variables are required.

Optional variables:
  UPSTREAM_VERSION_MODE=advisory|policy|strict
      advisory: report stale sources but exit successfully
      policy: fail only checks marked strict in config/upstream-versions.json
      strict: fail any stale or unavailable check
      Default: policy
  UPSTREAM_VERSION_ONLY=<id[,id...]>
      Limit checks to the named source IDs, or to source:release/source:commits
      comparison IDs.
  UPSTREAM_VERSION_FORMAT=text|github|json
      Output format. github emits workflow annotations.
      Default: text
  UPSTREAM_VERSION_SNAPSHOT=<path>
      Offline ls-remote snapshot for tests.
  V=0|1
      Show checked remote refs.

Each configured source can report two independent signals:
  release
      Whether a newer release tag exists than the recorded source checkpoint.
  commits
      Whether the tracked upstream branch head differs from the recorded source
      checkpoint. This is usually advisory because unreleased commits are less
      significant than tagged releases.
"""


@dataclass
class RemoteRef:
    oid: str
    ref: str


@dataclass
class LocalState:
    label: str
    version: str | None = None
    object_id: str | None = None


@dataclass
class UpstreamVersionResult:
    source_id: str
    check_id: str
    kind: str
    description: str
    mode: str
    status: str
    local: str
    remote: str
    detail: str


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--mode", default=os.environ.get("UPSTREAM_VERSION_MODE", "policy"), choices=("advisory", "policy", "strict"))
    p.add_argument("--only", default=os.environ.get("UPSTREAM_VERSION_ONLY", ""))
    p.add_argument("--format", default=os.environ.get("UPSTREAM_VERSION_FORMAT", "text"), choices=("text", "github", "json"))
    p.add_argument("--snapshot", default=os.environ.get("UPSTREAM_VERSION_SNAPSHOT", ""))
    p.add_argument("--v", default=os.environ.get("V", "0"), help="verbosity flag propagated from make")
    return p


def remote_url(repo: Path, remote_key: str) -> str:
    remotes = load_json(repo, "config/remotes.json").get("remotes", {})
    entry = remotes.get(remote_key)
    if not isinstance(entry, dict) or not entry.get("url"):
        raise ReconstructionError(f"config/remotes.json has no URL for remote {remote_key}")
    return str(entry["url"])


def load_snapshot(path: str) -> dict[str, list[RemoteRef]]:
    if not path:
        return {}
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    remotes = data.get("remotes", data)
    snapshot: dict[str, list[RemoteRef]] = {}
    for key, entries in remotes.items():
        refs: list[RemoteRef] = []
        for entry in entries:
            if isinstance(entry, dict):
                refs.append(RemoteRef(str(entry["oid"]), str(entry["ref"])))
            else:
                oid, ref = entry
                refs.append(RemoteRef(str(oid), str(ref)))
        snapshot[str(key)] = refs
    return snapshot


def ls_remote(repo: Path, remote_key: str, snapshot: dict[str, list[RemoteRef]], verbose: bool) -> list[RemoteRef]:
    if remote_key in snapshot:
        refs = snapshot[remote_key]
    else:
        url = remote_url(repo, remote_key)
        result = run(["git", "ls-remote", url], check=False)
        if result.returncode != 0:
            detail = (result.stderr or result.stdout or "").strip()
            raise ReconstructionError(f"remote unavailable for {remote_key}: {detail or url}")
        refs = []
        for line in result.stdout.splitlines():
            if not line.strip():
                continue
            oid, ref = line.split(None, 1)
            refs.append(RemoteRef(oid, ref))
    if verbose:
        for item in refs:
            print(f"[upstream-version] {remote_key}: {item.oid} {item.ref}")
    return refs


def normalise_version(value: str) -> str:
    return value.removeprefix("edk2-stable").removeprefix("v")


def ref_basename(ref: str) -> str:
    return ref.rsplit("/", 1)[-1]


def latest_record(records: list[dict[str, Any]], label: str) -> dict[str, Any]:
    if not records:
        raise ReconstructionError(f"no local {label} records are available")
    return sorted(records, key=lambda item: version_key(ref_basename(str(item.get("ref", "")))))[-1]


def local_edk2_release(repo: Path) -> LocalState:
    release = latest_value(matrix_release_values(repo), "EDK2")
    edk2_ref = f"edk2-stable{release}"
    records = [
        record
        for record in ref_manifest_records(repo, EDK2_REFS_MANIFEST)
        if record.get("component") == "edk2" and str(record.get("ref", "")).endswith(f"/{edk2_ref}")
    ]
    object_id = records[0].get("object_id") if records else None
    return LocalState(label=edk2_ref, version=release, object_id=str(object_id) if object_id else None)


def local_arm_release(repo: Path, component: str) -> LocalState:
    records = [record for record in ref_manifest_records(repo, ARM_REFS_MANIFEST) if record.get("component") == component]
    record = latest_record(records, f"Arm {component}")
    label = ref_basename(str(record["ref"]))
    return LocalState(
        label=label,
        version=normalise_version(label),
        object_id=str(record.get("object_id") or ""),
    )


def local_radxa_release(repo: Path) -> LocalState:
    release = latest_value(available_radxa_releases(repo), "Radxa")
    records = [
        record
        for record in ref_manifest_records(repo, RADXA_REFS_MANIFEST)
        if record.get("radxa_release") == release
    ]
    upstream_ref = next((record.get("upstream_ref") for record in records if record.get("upstream_ref")), None)
    return LocalState(label=release, version=release, object_id=str(upstream_ref) if upstream_ref else None)


def local_cix_component(repo: Path, component: str) -> LocalState:
    records = [record for record in ref_manifest_records(repo, CIX_REFS_MANIFEST) if record.get("component") == component]
    record = latest_record(records, f"CIX {component}")
    ref = str(record["ref"])
    release = ref.split("/")[3]
    return LocalState(label=f"cix-{release}/{component}", version=release, object_id=str(record.get("object_id") or ""))


def local_json_field(repo: Path, local: dict[str, Any]) -> LocalState:
    ref = str(local["ref"])
    path = str(local["manifest_path"])
    payload = json.loads(show_file(repo, ref, path).decode("utf-8"))
    label = str(payload[str(local["field"])])
    object_field = local.get("object_field")
    object_id = str(payload[object_field]) if object_field else None
    return LocalState(label=label, version=normalise_version(label), object_id=object_id)


def local_state(repo: Path, check: dict[str, Any]) -> LocalState:
    local = check.get("local", {})
    if not isinstance(local, dict):
        raise ReconstructionError(f"{check.get('id', '<unknown>')}: local must be an object")
    local_type = local.get("type")
    if local_type == "edk2-release":
        state = local_edk2_release(repo)
    elif local_type == "arm-base-release":
        state = local_arm_release(repo, str(local["component"]))
    elif local_type == "radxa-release":
        state = local_radxa_release(repo)
    elif local_type == "radxa-vendor-upstream-ref":
        state = local_radxa_release(repo)
        if not state.object_id:
            raise ReconstructionError("latest Radxa vendor source has no upstream_ref metadata")
    elif local_type == "cix-component-ref":
        state = local_cix_component(repo, str(local["component"]))
    elif local_type == "json-field":
        state = local_json_field(repo, local)
    else:
        raise ReconstructionError(f"unsupported version-check local type: {local_type}")

    if local.get("compare_object") is False:
        state.object_id = None
    return state


def latest_remote_tag(refs: list[RemoteRef], pattern: str) -> LocalState | None:
    regex = re.compile(pattern)
    peeled = {item.ref[:-3]: item.oid for item in refs if item.ref.endswith("^{}")}
    candidates: list[tuple[tuple[Any, ...], str, str, str]] = []
    for item in refs:
        if item.ref.endswith("^{}"):
            continue
        match = regex.match(item.ref)
        if not match:
            continue
        version = match.group("version")
        oid = peeled.get(item.ref, item.oid)
        label = ref_basename(item.ref)
        candidates.append((version_key(version), version, label, oid))
    if not candidates:
        return None
    _key, version, label, oid = sorted(candidates)[-1]
    return LocalState(label=label, version=version, object_id=oid)


def remote_branch_head(refs: list[RemoteRef], ref: str) -> LocalState | None:
    for item in refs:
        if item.ref == ref:
            return LocalState(label=ref, object_id=item.oid)
    return None


def compare_tag(local: LocalState, remote: LocalState) -> tuple[str, str]:
    if local.version is None or remote.version is None:
        raise ReconstructionError("tag version comparison requires local and remote versions")
    local_key = version_key(local.version)
    remote_key = version_key(remote.version)
    if remote_key > local_key:
        return "stale", f"remote has newer tag {remote.label}"
    if remote_key < local_key:
        return "ahead", f"local {local.label} is newer than remote {remote.label}"
    if local.object_id and remote.object_id and local.object_id != remote.object_id:
        return "changed", f"tag {remote.label} points at {remote.object_id}, local records {local.object_id}"
    return "current", "local record matches latest remote tag"


def compare_head(local: LocalState, remote: LocalState) -> tuple[str, str]:
    if not local.object_id:
        return "unknown", f"local {local.label} has no recorded upstream object"
    if remote.object_id != local.object_id:
        return "stale", f"remote head is {remote.object_id}, local records {local.object_id}"
    return "current", "local record matches remote branch head"


def comparison_items(check: dict[str, Any]) -> list[tuple[str, dict[str, Any]]]:
    """Return release/commit comparisons for one configured source."""

    if "kind" in check:
        label = "release" if check.get("kind") == "latest_tag" else "commits"
        merged = dict(check)
        merged.setdefault("comparison_id", str(check.get("id", "")))
        return [(label, merged)]

    items: list[tuple[str, dict[str, Any]]] = []
    for label, default_kind in (("release", "latest_tag"), ("commits", "branch_head")):
        item = check.get(label)
        if item is None:
            continue
        if not isinstance(item, dict):
            raise ReconstructionError(f"{check.get('id', '<unknown>')}: {label} must be an object")
        merged = {
            key: value
            for key, value in check.items()
            if key not in {"id", "release", "commits"}
        }
        merged.update(item)
        merged.setdefault("kind", default_kind)
        merged.setdefault("comparison_id", f"{check.get('id', '')}:{label}")
        items.append((label, merged))
    return items


def evaluate_comparison(
    repo: Path,
    check: dict[str, Any],
    kind_label: str,
    comparison: dict[str, Any],
    snapshot: dict[str, list[RemoteRef]],
    verbose: bool,
) -> UpstreamVersionResult:
    check_id = str(check.get("id", ""))
    comparison_id = str(comparison.get("comparison_id", f"{check_id}:{kind_label}"))
    description = str(comparison.get("description", check.get("description", comparison_id)))
    mode = str(comparison.get("mode", check.get("mode", "advisory")))
    local = local_state(repo, comparison)
    remote_key = str(comparison["remote"])
    try:
        refs = ls_remote(repo, remote_key, snapshot, verbose)
    except ReconstructionError as exc:
        return UpstreamVersionResult(
            check_id,
            comparison_id,
            kind_label,
            description,
            mode,
            "unavailable",
            local.label,
            "<unavailable>",
            str(exc),
        )

    kind = comparison.get("kind")
    if kind == "latest_tag":
        remote = latest_remote_tag(refs, str(comparison["tag_pattern"]))
        if remote is None:
            return UpstreamVersionResult(
                check_id,
                comparison_id,
                kind_label,
                description,
                mode,
                "unavailable",
                local.label,
                "<no matching tag>",
                "remote has no matching tags",
            )
        status, detail = compare_tag(local, remote)
    elif kind == "branch_head":
        remote = remote_branch_head(refs, str(comparison["ref"]))
        if remote is None:
            return UpstreamVersionResult(
                check_id,
                comparison_id,
                kind_label,
                description,
                mode,
                "unavailable",
                local.label,
                str(comparison["ref"]),
                "remote has no matching branch",
            )
        status, detail = compare_head(local, remote)
    else:
        raise ReconstructionError(f"{comparison_id}: unsupported version-check kind: {kind}")

    return UpstreamVersionResult(
        check_id,
        comparison_id,
        kind_label,
        description,
        mode,
        status,
        local.label,
        remote.label,
        detail,
    )


def should_fail(result: UpstreamVersionResult, mode: str) -> bool:
    if result.status in {"current", "ahead"}:
        return False
    if mode == "advisory":
        return False
    if mode == "strict":
        return True
    return result.mode == "strict"


def github_escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A").replace(":", "%3A").replace(",", "%2C")


def print_text(results: list[UpstreamVersionResult]) -> None:
    print("Upstream Versions")
    current_source = ""
    for result in results:
        if result.source_id != current_source:
            current_source = result.source_id
            print(f"- {result.source_id}:")
        print(f"  {result.kind}: {result.status} ({result.mode})")
        print(f"    local:  {result.local}")
        print(f"    remote: {result.remote}")
        print(f"    detail: {result.detail}")


def print_github(results: list[UpstreamVersionResult]) -> None:
    print_text(results)
    for result in results:
        if result.status in {"current", "ahead"}:
            continue
        title = github_escape(f"{result.check_id}: {result.status}")
        message = github_escape(f"{result.description}: {result.detail} (local {result.local}, remote {result.remote})")
        print(f"::warning title={title}::{message}")


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    snapshot = load_snapshot(args.snapshot)
    selected = {item.strip() for item in args.only.split(",") if item.strip()}
    manifest = load_json(repo, "config/upstream-versions.json")
    checks = manifest.get("checks", [])
    if not isinstance(checks, list) or not checks:
        raise ReconstructionError("config/upstream-versions.json has no checks")

    results = []
    for check in checks:
        if not isinstance(check, dict):
            raise ReconstructionError("config/upstream-versions.json checks must be objects")
        for kind_label, comparison in comparison_items(check):
            check_id = str(check.get("id", ""))
            comparison_id = str(comparison.get("comparison_id", f"{check_id}:{kind_label}"))
            if selected and check_id not in selected and comparison_id not in selected:
                continue
            results.append(evaluate_comparison(repo, check, kind_label, comparison, snapshot, verbose))

    if args.format == "json":
        print(json.dumps([result.__dict__ for result in results], indent=2, sort_keys=True))
    elif args.format == "github":
        print_github(results)
    else:
        print_text(results)

    failing = [result for result in results if should_fail(result, args.mode)]
    print(f"checked upstream versions in {format_duration(time.monotonic() - started)}")
    if failing:
        summary = "\n".join(f"- {result.check_id}: {result.detail}" for result in failing)
        raise ReconstructionError(f"upstream version check failed in {args.mode} mode:\n{summary}")


if __name__ == "__main__":
    main_wrapper(main)
