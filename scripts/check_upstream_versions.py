#!/usr/bin/env python3
"""Check whether recorded upstream/vendor source refs lag external remotes."""

from __future__ import annotations

import argparse
import json
import os
import re
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
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
    configured_remote_url,
    format_duration,
    latest_value,
    load_json,
    main_wrapper,
    matrix_release_values,
    ref_manifest_records,
    repo_root,
    run,
    selected_unofficial_current_ref,
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
      strict: fail any stale or unavailable non-advisory check
      Default: policy
  UPSTREAM_VERSION_ONLY=<id[,id...]>
      Limit checks to the named source IDs, or to source:release/source:commits
      comparison IDs.
  UPSTREAM_VERSION_FORMAT=text|github|json
      Output format. github emits workflow annotations and, when available,
      writes a GitHub job summary table.
      Default: text
  UPSTREAM_VERSION_SNAPSHOT=<path>
      Offline remote-ref snapshot for tests.
  V=0|1
      Show checked remote refs.

Each configured source or tooling pin can report two independent signals:
  release
      Whether a newer release tag or release-labelled commit exists than the
      recorded source branch.
  commits
      Whether the tracked upstream branch head differs from the recorded source
      branch. This is usually advisory because unreleased commits are less
      significant than tagged releases.
"""


@dataclass
class RemoteRef:
    oid: str
    ref: str
    subject: str = ""


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
                refs.append(RemoteRef(str(entry["oid"]), str(entry["ref"]), str(entry.get("subject", ""))))
            else:
                oid, ref = entry
                refs.append(RemoteRef(str(oid), str(ref)))
        snapshot[str(key)] = refs
    return snapshot


def ls_remote(repo: Path, remote_key: str, snapshot: dict[str, list[RemoteRef]], verbose: bool) -> list[RemoteRef]:
    if remote_key in snapshot:
        refs = snapshot[remote_key]
    else:
        url = configured_remote_url(repo, remote_key=remote_key)
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
    records = [
        record
        for record in ref_manifest_records(repo, CIX_REFS_MANIFEST)
        if record.get("component") == component
        and str(record.get("ref", "")).startswith("source/vendor/cix/")
    ]
    record = latest_record(records, f"CIX {component}")
    ref = str(record["ref"])
    release = ref.split("/")[3]
    return LocalState(label=f"cix-{release}/{component}", version=release, object_id=str(record.get("object_id") or ""))


def local_json_field(repo: Path, local: dict[str, Any]) -> LocalState:
    ref = (
        selected_unofficial_current_ref(repo)
        if local.get("ref") == "unofficial-default-current"
        else str(local["ref"])
    )
    path = str(local["manifest_path"])
    payload = json.loads(show_file(repo, ref, path).decode("utf-8"))
    label = str(payload[str(local["field"])])
    object_field = local.get("object_field")
    object_id = str(payload[object_field]) if object_field else None
    return LocalState(label=label, version=normalise_version(label), object_id=object_id)


def format_label(template: str, version: str, match: re.Match[str]) -> str:
    values = {key: value for key, value in match.groupdict().items() if value is not None}
    values.setdefault("version", version)
    return template.format(**values)


def local_file_regex(repo: Path, local: dict[str, Any]) -> LocalState:
    path = repo / str(local["path"])
    if not path.is_file():
        raise ReconstructionError(f"local version source does not exist: {path.relative_to(repo)}")
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(str(local["pattern"]), re.MULTILINE)
    match = pattern.search(text)
    if match is None:
        raise ReconstructionError(f"could not match local version pattern in {path.relative_to(repo)}")
    version_group = str(local.get("version_group", "version"))
    version = match.group(version_group)
    label = format_label(str(local.get("label_template", "{version}")), version, match)
    object_group = local.get("object_group")
    object_id = match.group(str(object_group)) if object_group else None
    return LocalState(label=label, version=normalise_version(version), object_id=object_id)


def local_workflow_action_ref(repo: Path, local: dict[str, Any]) -> LocalState:
    action = str(local["action"])
    workflow_root = repo / str(local.get("workflow_root", ".github/workflows"))
    if not workflow_root.is_dir():
        raise ReconstructionError(f"workflow directory does not exist: {workflow_root.relative_to(repo)}")

    pattern = re.compile(r"^\s*-?\s*uses:\s+['\"]?" + re.escape(action) + r"@(?P<ref>[^'\"\s#]+)", re.MULTILINE)
    refs: dict[str, list[str]] = {}
    for path in sorted(workflow_root.glob("*.y*ml")):
        text = path.read_text(encoding="utf-8")
        for match in pattern.finditer(text):
            refs.setdefault(match.group("ref"), []).append(str(path.relative_to(repo)))

    if not refs:
        raise ReconstructionError(f"no workflow action reference found for {action}")
    if len(refs) > 1:
        detail = ", ".join(f"{ref} in {', '.join(paths)}" for ref, paths in sorted(refs.items()))
        raise ReconstructionError(f"workflow action {action} has inconsistent refs: {detail}")

    ref = next(iter(refs))
    return LocalState(label=ref, version=normalise_version(ref))


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
    elif local_type == "file-regex":
        state = local_file_regex(repo, local)
    elif local_type == "workflow-action-ref":
        state = local_workflow_action_ref(repo, local)
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


def latest_remote_subject_from_snapshot(refs: list[RemoteRef], ref: str, pattern: str) -> LocalState | None:
    regex = re.compile(pattern)
    historical_prefix = f"{ref}~"
    for item in refs:
        if item.ref != ref and not item.ref.startswith(historical_prefix):
            continue
        if not item.subject:
            continue
        if regex.match(item.subject):
            return LocalState(label=item.subject, object_id=item.oid)
    return None


def latest_remote_subject(
    repo: Path,
    remote_key: str,
    ref: str,
    pattern: str,
    scan_depth: int,
    snapshot: dict[str, list[RemoteRef]],
    verbose: bool,
) -> LocalState | None:
    if remote_key in snapshot:
        return latest_remote_subject_from_snapshot(snapshot[remote_key], ref, pattern)

    if scan_depth <= 0:
        raise ReconstructionError(f"{remote_key}: release_subject scan_depth must be greater than zero")

    regex = re.compile(pattern)
    url = configured_remote_url(repo, remote_key=remote_key)
    with tempfile.TemporaryDirectory(prefix=f"upstream-version-{remote_key}-") as tmp:
        work = Path(tmp)
        run(["git", "init", "--quiet", str(work)])
        result = run(["git", "-C", str(work), "fetch", "--quiet", "--depth", str(scan_depth), url, ref], check=False)
        if result.returncode != 0:
            detail = (result.stderr or result.stdout or "").strip()
            raise ReconstructionError(f"remote unavailable for {remote_key}: {detail or url}")
        log = run(
            [
                "git",
                "-C",
                str(work),
                "log",
                "--format=%H%x00%s",
                f"-n{scan_depth}",
                "FETCH_HEAD",
            ]
        )

    for line in log.stdout.splitlines():
        if "\x00" not in line:
            continue
        oid, subject = line.split("\x00", 1)
        if verbose:
            print(f"[upstream-version] {remote_key}: {oid} {ref} {subject}")
        if regex.match(subject):
            return LocalState(label=subject, object_id=oid)
    return None


def latest_docker_tag_from_names(tags: list[str], tag_pattern: str) -> LocalState | None:
    regex = re.compile(tag_pattern)
    candidates: list[LocalState] = []
    for tag in tags:
        match = regex.match(tag)
        if match is None:
            continue
        version = match.groupdict().get("version", tag)
        candidates.append(LocalState(label=tag, version=normalise_version(version)))
    return sorted(candidates, key=lambda item: version_key(item.version or item.label))[-1] if candidates else None


def latest_docker_tag_from_snapshot(refs: list[RemoteRef], tag_pattern: str) -> LocalState | None:
    tags: list[str] = []
    for item in refs:
        for prefix in ("docker://", "docker-tag://"):
            if item.ref.startswith(prefix):
                tags.append(item.ref.removeprefix(prefix))
                break
    return latest_docker_tag_from_names(tags, tag_pattern)


def parse_www_authenticate(value: str) -> dict[str, str]:
    if not value.startswith("Bearer "):
        return {}
    params: dict[str, str] = {}
    for match in re.finditer(r'([A-Za-z_][A-Za-z0-9_-]*)="([^"]*)"', value[len("Bearer ") :]):
        params[match.group(1)] = match.group(2)
    return params


def registry_bearer_token(challenge: str, repository: str, registry: str) -> str | None:
    params = parse_www_authenticate(challenge)
    realm = params.get("realm")
    if not realm:
        return None
    query = {
        "service": params.get("service", registry),
        "scope": params.get("scope", f"repository:{repository}:pull"),
    }
    url = f"{realm}?{urllib.parse.urlencode(query)}"
    with urllib.request.urlopen(url, timeout=30) as response:
        payload = json.loads(response.read().decode("utf-8"))
    token = payload.get("token") or payload.get("access_token")
    return str(token) if token else None


def fetch_docker_tags(registry: str, repository: str, name_filter: str = "") -> list[str]:
    query = {"page_size": "100"}
    if name_filter:
        query["name"] = name_filter
    url: str | None = f"https://hub.docker.com/v2/repositories/{repository}/tags?{urllib.parse.urlencode(query)}"
    tags: list[str] = []
    while url:
        try:
            with urllib.request.urlopen(url, timeout=30) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except urllib.error.URLError as exc:
            raise ReconstructionError(f"container tag list unavailable for {repository}: {exc}") from exc
        for item in payload.get("results", []):
            if isinstance(item, dict) and item.get("name"):
                tags.append(str(item["name"]))
        next_url = payload.get("next")
        url = str(next_url) if next_url else None
    return tags


def parse_link_next(value: str | None) -> str | None:
    if not value:
        return None
    for part in value.split(","):
        url_part, sep, rel_part = part.partition(";")
        if sep and 'rel="next"' in rel_part and url_part.strip().startswith("<") and url_part.strip().endswith(">"):
            return url_part.strip()[1:-1]
    return None


def registry_request_json(url: str, registry: str, repository: str) -> tuple[dict[str, Any], str | None]:
    headers = {"Accept": "application/json", "User-Agent": "edk2-cix-upstream-version-check"}

    def request(extra_headers: dict[str, str] | None = None) -> urllib.response.addinfourl:
        merged = dict(headers)
        if extra_headers:
            merged.update(extra_headers)
        return urllib.request.urlopen(urllib.request.Request(url, headers=merged), timeout=30)

    try:
        response = request()
    except urllib.error.HTTPError as exc:
        if exc.code != 401:
            raise ReconstructionError(f"container tag list unavailable for {repository}: {exc}") from exc
        token = registry_bearer_token(exc.headers.get("WWW-Authenticate", ""), repository, registry)
        if not token:
            raise ReconstructionError(f"container registry authentication failed for {repository}") from exc
        response = request({"Authorization": f"Bearer {token}"})
    except urllib.error.URLError as exc:
        raise ReconstructionError(f"container tag list unavailable for {repository}: {exc}") from exc

    with response:
        payload = json.loads(response.read().decode("utf-8"))
        return payload, response.headers.get("Link")


def fetch_oci_registry_tags(registry: str, repository: str) -> list[str]:
    tags: list[str] = []
    url: str | None = f"https://{registry}/v2/{repository}/tags/list?{urllib.parse.urlencode({'n': '1000'})}"
    while url:
        if url.startswith("/"):
            url = f"https://{registry}{url}"
        payload, link = registry_request_json(url, registry, repository)
        tags.extend(str(tag) for tag in payload.get("tags", []) if tag)
        url = parse_link_next(link)
    return tags


def fetch_container_tags(registry: str, repository: str, name_filter: str = "") -> list[str]:
    if registry in {"registry-1.docker.io", "docker.io", "index.docker.io"}:
        return fetch_docker_tags(registry, repository, name_filter)
    return fetch_oci_registry_tags(registry, repository)


def remote_docker_latest_tag(
    repo: Path,
    remote_key: str,
    tag_pattern: str,
    name_filter: str,
    snapshot: dict[str, list[RemoteRef]],
    verbose: bool,
) -> LocalState | None:
    if remote_key in snapshot:
        return latest_docker_tag_from_snapshot(snapshot[remote_key], tag_pattern)

    remotes = load_json(repo, "config/remotes.json").get("remotes", {})
    entry = remotes.get(remote_key)
    if not isinstance(entry, dict):
        raise ReconstructionError(f"unknown remote key: {remote_key}")
    registry = str(entry.get("registry", "registry-1.docker.io"))
    repository = str(entry.get("repository", ""))
    if not repository:
        raise ReconstructionError(f"{remote_key}: container-image remote requires a repository")
    latest = latest_docker_tag_from_names(fetch_container_tags(registry, repository, name_filter), tag_pattern)
    if latest is not None:
        display_repository = repository if registry in {"registry-1.docker.io", "docker.io", "index.docker.io"} else f"{registry}/{repository}"
        latest.label = f"{display_repository}:{latest.label}"
        if verbose:
            print(f"[upstream-version] {remote_key}: docker://{latest.version}")
    return latest


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


def advisory_branch_head_detail(comparison_id: str, local: LocalState, remote: LocalState) -> str:
    if comparison_id == "radxa:commits":
        return (
            f"upstream {remote.label} has commits beyond the latest recorded Radxa "
            f"release {local.label}; remote head is {remote.object_id}, "
            f"local release tag records {local.object_id}"
        )
    return (
        f"upstream {remote.label} has unreleased commits beyond the recorded local "
        f"source {local.label}; remote head is {remote.object_id}, "
        f"local records {local.object_id}"
    )


def compare_object(local: LocalState, remote: LocalState, remote_label: str) -> tuple[str, str]:
    if not local.object_id:
        return "unknown", f"local {local.label} has no recorded upstream object"
    if remote.object_id != local.object_id:
        return "stale", f"{remote_label} is {remote.object_id}, local records {local.object_id}"
    return "current", f"local record matches {remote_label}"


def comparison_items(check: dict[str, Any]) -> list[tuple[str, dict[str, Any]]]:
    """Return release/commit comparisons for one configured source."""

    if "kind" in check:
        label = "release" if check.get("kind") in {"latest_tag", "release_subject", "docker_latest_tag"} else "commits"
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
    kind = comparison.get("kind")
    if kind not in {"latest_tag", "branch_head", "release_subject", "docker_latest_tag"}:
        raise ReconstructionError(f"{comparison_id}: unsupported version-check kind: {kind}")
    try:
        if kind == "latest_tag":
            refs = ls_remote(repo, remote_key, snapshot, verbose)
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
            refs = ls_remote(repo, remote_key, snapshot, verbose)
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
            if status == "stale" and mode == "advisory":
                status = "unreleased"
                detail = advisory_branch_head_detail(comparison_id, local, remote)
        elif kind == "release_subject":
            remote = latest_remote_subject(
                repo,
                remote_key,
                str(comparison["ref"]),
                str(comparison["subject_pattern"]),
                int(comparison.get("scan_depth", 64)),
                snapshot,
                verbose,
            )
            if remote is None:
                return UpstreamVersionResult(
                    check_id,
                    comparison_id,
                    kind_label,
                    description,
                    mode,
                    "unavailable",
                    local.label,
                    "<no matching release subject>",
                    "remote has no matching release subject",
                )
            status, detail = compare_object(local, remote, "release commit")
        elif kind == "docker_latest_tag":
            remote = remote_docker_latest_tag(
                repo,
                remote_key,
                str(comparison["tag_pattern"]),
                str(comparison.get("tag_name_filter", "")),
                snapshot,
                verbose,
            )
            if remote is None:
                return UpstreamVersionResult(
                    check_id,
                    comparison_id,
                    kind_label,
                    description,
                    mode,
                    "unavailable",
                    local.label,
                    "<no matching container image tag>",
                    "remote has no matching container image tags",
                )
            status, detail = compare_tag(local, remote)
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
    if result.status in {"current", "ahead", "unreleased"}:
        return False
    if mode == "advisory" or result.mode == "advisory":
        return False
    if mode == "strict":
        return True
    return result.mode == "strict"


def github_escape(value: str) -> str:
    return value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A").replace(":", "%3A").replace(",", "%2C")


def markdown_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace("|", "\\|").replace("\r\n", "\n").replace("\n", "<br>")


def write_github_summary(results: list[UpstreamVersionResult], path: str | Path) -> None:
    lines = ["## Upstream Version Check", ""]
    if not results:
        lines.append("No upstream version checks were selected.")
    else:
        lines.extend(
            [
                "| Source | Check | Status | Policy | Local | Remote | Detail |",
                "| --- | --- | --- | --- | --- | --- | --- |",
            ]
        )
        for result in results:
            lines.append(
                "| "
                + " | ".join(
                    markdown_escape(value)
                    for value in (
                        result.source_id,
                        result.kind,
                        result.status,
                        result.mode,
                        result.local,
                        result.remote,
                        result.detail,
                    )
                )
                + " |"
            )
    with Path(path).open("a", encoding="utf-8") as f:
        f.write("\n".join(lines))
        f.write("\n")


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
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        write_github_summary(results, summary_path)
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
