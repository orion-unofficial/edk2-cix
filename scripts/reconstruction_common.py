#!/usr/bin/env python3
"""Shared helpers for the EDK2-CIX firmware source tooling."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
import traceback
from copy import deepcopy
from pathlib import Path
from typing import Any, Iterable


class ReconstructionError(RuntimeError):
    """Raised for expected user-facing workflow failures."""


_REF_MANIFEST_RECORD_CACHE: dict[tuple[str, str], list[dict[str, Any]]] = {}
_RENDERED_REF_RECORD_CACHE: dict[str, dict[str, dict[str, Any]]] = {}
_BASE_TREE_RECORD_CACHE: dict[str, dict[str, dict[str, Any]]] = {}


def clear_metadata_caches() -> None:
    _REF_MANIFEST_RECORD_CACHE.clear()
    _RENDERED_REF_RECORD_CACHE.clear()
    _BASE_TREE_RECORD_CACHE.clear()


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


def manifest_record_sort_key(item: dict[str, Any]) -> str:
    if item.get("ref"):
        return str(item["ref"])
    refs = item.get("refs")
    if isinstance(refs, list) and refs:
        return str(refs[0])
    return ""


def manifest_defaults(data: dict[str, Any]) -> dict[str, Any]:
    """Return defaults from a manifest, accepting either an object or objects list."""

    defaults = data.get("defaults", {})
    if isinstance(defaults, list):
        merged: dict[str, Any] = {}
        for item in defaults:
            if isinstance(item, dict):
                merged.update(item)
        return merged
    if isinstance(defaults, dict):
        return dict(defaults)
    return {}


def records_from_manifest(repo: Path, relative: str) -> list[dict[str, Any]]:
    data = load_json(repo, relative)
    defaults = manifest_defaults(data)
    records: list[dict[str, Any]] = []
    if "releases" in data:
        records.extend(records_from_release_manifest(relative, data, defaults))
    for item in data.get("refs", []):
        records.extend(expand_ref_manifest_item(relative, defaults, item))
    derive_manifest_fields(records)
    return records


def expand_ref_manifest_item(relative: str, defaults: dict[str, Any], item: dict[str, Any]) -> list[dict[str, Any]]:
    item_refs = item.get("refs")
    if item_refs is None:
        refs = [item.get("ref")]
    elif isinstance(item_refs, list):
        refs = item_refs
    else:
        raise ReconstructionError(f"{relative}: refs must be an array when present")

    records: list[dict[str, Any]] = []
    for ref in refs:
        if not ref:
            raise ReconstructionError(f"{relative}: manifest record is missing ref")
        record = deepcopy(defaults)
        record.update(deepcopy(item))
        record.pop("refs", None)
        record["ref"] = ref
        templates = defaults.get("component_templates")
        if templates and "components" not in item and record.get("ref"):
            edk2_ref = str(record["ref"]).rsplit("/", 1)[-1]
            record["components"] = [
                {
                    key: str(value).format(edk2_ref=edk2_ref)
                    for key, value in template.items()
                }
                for template in templates
            ]
        record.pop("component_templates", None)
        records.append(record)
    return records


def format_manifest_value(value: Any, context: dict[str, Any]) -> Any:
    if isinstance(value, str):
        return value.format(**context)
    if isinstance(value, list):
        return [format_manifest_value(item, context) for item in value]
    if isinstance(value, dict):
        return {key: format_manifest_value(item, context) for key, item in value.items()}
    return value


def records_from_release_manifest(
    relative: str,
    data: dict[str, Any],
    defaults: dict[str, Any],
) -> list[dict[str, Any]]:
    templates = data.get("component_templates")
    if not isinstance(templates, dict):
        raise ReconstructionError(f"{relative}: releases require component_templates object")

    records: list[dict[str, Any]] = []
    for release in data.get("releases", []):
        if not isinstance(release, dict):
            raise ReconstructionError(f"{relative}: release records must be objects")
        edk2_ref = release.get("edk2_ref")
        components = release.get("components")
        if not isinstance(edk2_ref, str) or not edk2_ref:
            raise ReconstructionError(f"{relative}: release record is missing edk2_ref")
        if not isinstance(components, dict):
            raise ReconstructionError(f"{relative}: release {edk2_ref} is missing components")

        release_context = {
            key: value
            for key, value in release.items()
            if key != "components"
        }
        for component, component_data in components.items():
            if component not in templates:
                raise ReconstructionError(f"{relative}: no component template for {component}")
            if not isinstance(component_data, dict):
                raise ReconstructionError(f"{relative}: component {component} in {edk2_ref} must be an object")
            context = dict(release_context)
            context["component"] = component
            record = deepcopy(defaults)
            record.update(format_manifest_value(templates[component], context))
            record.update(deepcopy(component_data))
            record["component"] = component
            records.append(record)
    return records


def derive_manifest_fields(records: list[dict[str, Any]]) -> None:
    for record in records:
        ref = str(record.get("ref", ""))
        match = RADXA_SOURCE_RE.match(ref)
        if not match:
            continue
        source = match.group("source")
        radxa = match.group("radxa")
        edk2 = match.group("edk2")
        record.setdefault("vendor", "radxa")
        record.setdefault("format", "materialised source tree")
        record.setdefault("radxa_release", radxa)
        record.setdefault("edk2_base", edk2)
        record.setdefault("base_ref", f"{CACHE_BASE_EDK2_PREFIX}{edk2}")
        record.setdefault(
            "type",
            "vendor-source" if source == "vendor" else "ported-vendor-source",
        )

    previous_by_radxa: dict[str, str] = {}
    radxa_records = [
        record
        for record in records
        if RADXA_SOURCE_RE.match(str(record.get("ref", "")))
    ]
    radxa_records.sort(
        key=lambda record: (
            version_key(str(record.get("radxa_release", ""))),
            version_key(str(record.get("edk2_base", ""))),
            str(record.get("ref", "")),
        )
    )
    for record in radxa_records:
        ref = str(record["ref"])
        radxa = str(record["radxa_release"])
        match = RADXA_SOURCE_RE.match(ref)
        if match and match.group("source") == "port" and radxa in previous_by_radxa:
            record.setdefault("ported_from", previous_by_radxa[radxa])
        previous_by_radxa[radxa] = ref


def ref_manifest_records(repo: Path, manifest_name: str) -> list[dict[str, Any]]:
    key = (str(repo.resolve()), manifest_name)
    if key not in _REF_MANIFEST_RECORD_CACHE:
        _REF_MANIFEST_RECORD_CACHE[key] = records_from_manifest(repo, f"config/{manifest_name}")
    return _REF_MANIFEST_RECORD_CACHE[key]


def truthy(value: str | int | bool | None) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return False
    return str(value).strip().lower() in {"1", "true", "yes", "on"}


def format_duration(seconds: float) -> str:
    if seconds < 1:
        return f"{seconds * 1000:.0f} ms"
    if seconds < 60:
        return f"{seconds:.1f} s"
    minutes, remainder = divmod(seconds, 60)
    return f"{int(minutes)} min {remainder:.1f} s"


RELEASE_STAGE_PREFIXES = ("custom", "vendor", "upstream")
UNBUILDABLE_RADXA_RELEASES = {"0.1.1-1"}
MIN_SUPPORTED_EDK2_RELEASE = "202208"
ARM_REFS_MANIFEST = "refs-arm.json"
CIX_REFS_MANIFEST = "refs-cix.json"
EDK2_REFS_MANIFEST = "refs-edk2.json"
RADXA_REFS_MANIFEST = "refs-radxa.json"
VARIANT_CACHE_MANIFEST = "refs-variant-cache.json"
CACHE_REF_PREFIX = "source/cache/"
CACHE_RELEASE_PREFIX = "source/cache/release/"
CACHE_BASE_EDK2_PREFIX = "source/cache/base/edk2/"


def version_key(value: str) -> tuple[Any, ...]:
    """Sort release-like values using numeric components where possible."""

    parts: list[Any] = []
    for token in re.findall(r"\d+|[A-Za-z]+|[^A-Za-z0-9]+", value):
        if token.isdigit():
            parts.append((0, int(token)))
        elif token.isalpha():
            parts.append((1, token.lower()))
        else:
            parts.append((2, token))
    return tuple(parts)


def latest_value(values: Iterable[str], label: str) -> str:
    values = sorted(set(values), key=version_key)
    if not values:
        raise ReconstructionError(f"no {label} releases are available")
    return values[-1]


def source_ref_candidates(ref: str) -> list[str]:
    """Return equivalent local and remote-tracking names for source refs."""

    if ref.startswith("refs/"):
        return [ref]
    if not ref.startswith("source/"):
        return [ref]
    return [f"refs/heads/{ref}", ref, f"refs/remotes/origin/{ref}"]


def resolve_ref(repo: Path, ref: str, check: bool = True) -> str | None:
    """Resolve a ref, accepting origin/source/** when local source heads are absent."""

    for candidate in source_ref_candidates(ref):
        result = git(
            repo,
            "rev-parse",
            "--verify",
            "--quiet",
            f"{candidate}^{{commit}}",
            check=False,
        )
        if result.returncode == 0:
            return candidate
    if check:
        raise ReconstructionError(f"ref is unavailable locally: {ref}")
    return None


def for_each_ref(repo: Path, namespace: str) -> list[str]:
    refs: set[str] = set()
    prefixes = [
        ("refs/heads/", f"refs/heads/{namespace}"),
        ("refs/remotes/origin/", f"refs/remotes/origin/{namespace}"),
    ]
    for strip_prefix, query in prefixes:
        result = git(repo, "for-each-ref", "--format=%(refname)", query, check=False)
        if result.returncode != 0:
            continue
        for line in result.stdout.splitlines():
            if line.startswith(strip_prefix):
                refs.add(line[len(strip_prefix) :])
    return sorted(refs, key=version_key)


def release_to_branch(release: str) -> str:
    if release.startswith("refs/heads/"):
        release = release[len("refs/heads/") :]
    if release.startswith(CACHE_RELEASE_PREFIX):
        return release
    first, _, _rest = release.partition("/")
    if first in RELEASE_STAGE_PREFIXES:
        return f"{CACHE_RELEASE_PREFIX}{release}"
    if "/unofficial" in release:
        return f"{CACHE_RELEASE_PREFIX}custom/{release}"
    if "/cix-" in release:
        return f"{CACHE_RELEASE_PREFIX}vendor/{release}"
    return f"{CACHE_RELEASE_PREFIX}{release}"


def branch_to_ref(branch: str) -> str:
    return branch if branch.startswith("refs/") else f"refs/heads/{branch}"


def short_release(branch: str) -> str:
    return branch[len(CACHE_RELEASE_PREFIX) :] if branch.startswith(CACHE_RELEASE_PREFIX) else branch


def variant_name(branch: str) -> str:
    """Return the user-facing source-target name for a generated release cache branch."""

    short = short_release(branch)
    first, sep, rest = short.partition("/")
    if sep and first in RELEASE_STAGE_PREFIXES:
        return rest
    return short


EDK2_BASE_REF_RE = re.compile(
    r"^source/base/edk2/edk2-stable(?P<release>\d{6}(?:\.\d+)?)$"
)


def matrix_release_values(repo: Path) -> list[str]:
    """Return supported EDK2 releases from available base refs."""

    values: set[str] = set()
    for ref in for_each_ref(repo, "source/base/edk2"):
        match = EDK2_BASE_REF_RE.match(ref)
        if not match:
            continue
        release = match.group("release")
        if version_key(release) >= version_key(MIN_SUPPORTED_EDK2_RELEASE):
            values.add(release)
    if values:
        return sorted(values, key=version_key)

    raise ReconstructionError(
        "no supported EDK2 base refs are available; expected "
        "source/base/edk2/edk2-stable* refs from 202208 onward"
    )


def edk2_ref_for_release(release: str) -> str:
    return f"edk2-stable{release}"


def release_for_edk2_ref(edk2_ref: str) -> str:
    return edk2_ref.removeprefix("edk2-stable")


def expand_matrix_template(template: str, release: str) -> str:
    return template.format(release=release, edk2_ref=edk2_ref_for_release(release))


def matrix_variant_releases(variant: dict[str, Any], all_releases: list[str]) -> list[str]:
    releases = variant.get("releases")
    if releases == "all":
        return all_releases
    if isinstance(releases, list):
        return releases
    if "release" in variant:
        return [variant["release"]]
    raise ReconstructionError(f"variant has no release selection: {variant.get('name', '<unnamed>')}")


RADXA_SOURCE_RE = re.compile(r"^source/(?P<source>vendor|port)/radxa/(?P<radxa>[^/]+)/(?P<edk2>edk2-stable[^/]+)$")
CIX_COMPONENT_RE = re.compile(r"^source/component/cix/(?P<release>[^/]+)/(?P<component>[^/]+)$")
LOCAL_COMPAT_RE = re.compile(r"^source/unofficial/(?P<edk2>edk2-stable[^/]+)$")
LOCAL_COMPAT_TAG_RE = re.compile(r"^source/unofficial/edk2/stable-(?P<release>\d{6}(?:\.\d+)?)$")


def radxa_source_namespaces() -> tuple[str, ...]:
    return ("source/vendor/radxa", "source/port/radxa")


def radxa_source_ref(repo: Path, radxa: str, edk2_ref: str) -> str:
    """Return the canonical Radxa source ref for a selected EDK2 base."""

    candidates = [
        f"source/vendor/radxa/{radxa}/{edk2_ref}",
        f"source/port/radxa/{radxa}/{edk2_ref}",
    ]
    for ref in candidates:
        if ref_exists(repo, ref):
            return ref
    raise ReconstructionError(
        f"no Radxa source ref recorded for Radxa {radxa} on {edk2_ref}; "
        f"expected one of: {', '.join(candidates)}"
    )


def radxa_source_refs(repo: Path) -> list[str]:
    refs: list[str] = []
    for namespace in radxa_source_namespaces():
        refs.extend(ref for ref in for_each_ref(repo, namespace) if RADXA_SOURCE_RE.match(ref))
    return sorted(refs, key=version_key)


def radxa_releases_by_edk2(repo: Path, supported_edk2_refs: Iterable[str] | None = None) -> dict[str, list[str]]:
    supported = set(supported_edk2_refs or [])
    releases: dict[str, set[str]] = {}
    for ref in radxa_source_refs(repo):
        match = RADXA_SOURCE_RE.match(ref)
        if not match:
            continue
        radxa = match.group("radxa")
        edk2 = match.group("edk2")
        if radxa in UNBUILDABLE_RADXA_RELEASES:
            continue
        if supported and edk2 not in supported:
            continue
        releases.setdefault(edk2, set()).add(radxa)
    return {edk2: sorted(values, key=version_key) for edk2, values in sorted(releases.items(), key=lambda item: version_key(item[0]))}


def available_radxa_releases(repo: Path, supported_edk2_refs: Iterable[str] | None = None) -> list[str]:
    releases: set[str] = set()
    for values in radxa_releases_by_edk2(repo, supported_edk2_refs).values():
        releases.update(values)
    return sorted(releases, key=version_key)


def cix_release_components(repo: Path) -> dict[str, set[str]]:
    releases: dict[str, set[str]] = {}
    for ref in for_each_ref(repo, "source/component/cix"):
        match = CIX_COMPONENT_RE.match(ref)
        if not match:
            continue
        releases.setdefault(match.group("release"), set()).add(match.group("component"))
    return releases


def available_cix_releases(repo: Path) -> list[str]:
    releases = [
        release
        for release, components in cix_release_components(repo).items()
        if {"tf-a", "op-tee"}.issubset(components)
    ]
    return sorted(releases, key=version_key)


def local_compatibility_refs(repo: Path) -> list[str]:
    refs: list[str] = []
    for ref in for_each_ref(repo, "source/unofficial"):
        if LOCAL_COMPAT_RE.match(ref):
            refs.append(ref)
    return sorted(refs, key=version_key)


def local_compatibility_edk2_refs(repo: Path) -> set[str]:
    refs: set[str] = set()
    for ref in local_compatibility_refs(repo):
        match = LOCAL_COMPAT_RE.match(ref)
        if match:
            refs.add(match.group("edk2"))
    return refs


def local_compatibility_tag_for_branch(branch: str) -> str:
    match = LOCAL_COMPAT_RE.match(branch)
    if not match:
        raise ReconstructionError(f"not an unofficial compatibility branch: {branch}")
    release = release_for_edk2_ref(match.group("edk2"))
    return f"source/unofficial/edk2/stable-{release}"


def local_compatibility_branch_for_tag(tag: str) -> str:
    match = LOCAL_COMPAT_TAG_RE.match(tag)
    if not match:
        raise ReconstructionError(f"not an unofficial compatibility tag: {tag}")
    return f"source/unofficial/{edk2_ref_for_release(match.group('release'))}"


def matrix_release_branches(repo: Path) -> tuple[set[str], dict[str, str]]:
    all_releases = matrix_release_values(repo)
    supported_edk2 = {edk2_ref_for_release(release) for release in all_releases}
    radxa_by_edk2 = radxa_releases_by_edk2(repo, supported_edk2)
    cix_releases = available_cix_releases(repo)
    unofficial_edk2 = local_compatibility_edk2_refs(repo)
    branches: set[str] = set()
    aliases: dict[str, str] = {}

    for release in all_releases:
        edk2_ref = edk2_ref_for_release(release)
        for radxa in radxa_by_edk2.get(edk2_ref, []):
            upstream = f"{CACHE_RELEASE_PREFIX}upstream/edk2-{release}/radxa-{radxa}"
            branches.add(upstream)

            if edk2_ref in unofficial_edk2:
                unofficial = f"{CACHE_RELEASE_PREFIX}custom/edk2-{release}/radxa-{radxa}/unofficial"
                alias = f"{unofficial}-{radxa}"
                branches.update({unofficial, alias})
                aliases[alias] = unofficial

            for cix in cix_releases:
                vendor = f"{CACHE_RELEASE_PREFIX}vendor/edk2-{release}/cix-{cix}/radxa-{radxa}"
                branches.add(vendor)
                if edk2_ref in unofficial_edk2:
                    unofficial = f"{CACHE_RELEASE_PREFIX}custom/edk2-{release}/cix-{cix}/radxa-{radxa}/unofficial"
                    alias = f"{unofficial}-{radxa}"
                    branches.update({unofficial, alias})
                    aliases[alias] = unofficial

    return branches, aliases


def rendered_ref_records(repo: Path) -> dict[str, dict[str, Any]]:
    cache_key = str(repo.resolve())
    if cache_key in _RENDERED_REF_RECORD_CACHE:
        return _RENDERED_REF_RECORD_CACHE[cache_key]
    path = repo / "config" / VARIANT_CACHE_MANIFEST
    if not path.exists():
        return {}
    records = ref_manifest_records(repo, VARIANT_CACHE_MANIFEST)
    by_ref = {record["ref"]: record for record in records if record.get("ref")}
    for ref in radxa_source_refs(repo):
        match = RADXA_SOURCE_RE.match(ref)
        if not match:
            continue
        release = release_for_edk2_ref(match.group("edk2"))
        radxa = match.group("radxa")
        branch = f"{CACHE_RELEASE_PREFIX}upstream/edk2-{release}/radxa-{radxa}"
        by_ref.setdefault(
            branch,
            {
                "ref": branch,
                "immutable": True,
                "type": "rendered-upstream-radxa-release",
                "tree_id": tree_id(repo, ref),
                "derived_from": ref,
            },
        )
    for ref, record in list(by_ref.items()):
        try:
            parts = release_branch_parts(ref)
        except ReconstructionError:
            continue
        if parts.get("unofficial") != "unofficial":
            continue
        alias = f"{ref}-{parts['radxa']}"
        if alias in by_ref:
            continue
        alias_record = deepcopy(record)
        alias_record["ref"] = alias
        alias_record["alias_of"] = ref
        by_ref[alias] = alias_record
    _RENDERED_REF_RECORD_CACHE[cache_key] = by_ref
    return by_ref


def base_tree_records(repo: Path) -> dict[str, dict[str, Any]]:
    cache_key = str(repo.resolve())
    if cache_key in _BASE_TREE_RECORD_CACHE:
        return _BASE_TREE_RECORD_CACHE[cache_key]
    records: dict[str, dict[str, Any]] = {}
    grouped: dict[str, dict[str, str]] = {}
    paths = {
        "edk2": "src/edk2",
        "edk2-platforms": "src/edk2-platforms",
        "edk2-non-osi": "src/edk2-non-osi",
    }
    for record in ref_manifest_records(repo, EDK2_REFS_MANIFEST):
        component = record.get("component")
        ref = record.get("ref")
        if component not in paths or not isinstance(ref, str):
            continue
        edk2_ref = ref.rsplit("/", 1)[-1]
        grouped.setdefault(edk2_ref, {})[component] = ref

    for edk2_ref, components_by_name in sorted(grouped.items(), key=lambda item: version_key(item[0])):
        if set(paths) - set(components_by_name):
            continue
        components = [
            {"path": path, "ref": components_by_name[component]}
            for component, path in paths.items()
        ]
        ref = f"{CACHE_BASE_EDK2_PREFIX}{edk2_ref}"
        records[ref] = {
            "ref": ref,
            "component": "rendered-edk2-base",
            "components": components,
            "immutable": True,
            "tree_id": component_skeleton_tree(repo, components),
            "type": "component-skeleton",
        }
    _BASE_TREE_RECORD_CACHE[cache_key] = records
    return records


RELEASE_BRANCH_RE = re.compile(
    rf"^{re.escape(CACHE_RELEASE_PREFIX)}"
    r"(?P<stage>upstream|vendor|custom)/"
    r"edk2-(?P<release>\d{6}(?:\.\d+)?)/"
    r"(?:(?:cix-(?P<cix>[^/]+)/)?)"
    r"radxa-(?P<radxa>[^/]+)"
    r"(?:/(?P<unofficial>unofficial(?:-[^/]+)?))?"
    r"$"
)


def release_branch_parts(branch: str) -> dict[str, str]:
    match = RELEASE_BRANCH_RE.match(branch)
    if not match:
        raise ReconstructionError(f"cannot derive render plan from unsupported source-target branch name: {branch}")
    parts = {k: v for k, v in match.groupdict().items() if v}
    stage = parts["stage"]
    if stage == "upstream" and (parts.get("cix") or parts.get("unofficial")):
        raise ReconstructionError(f"invalid upstream source-target branch: {branch}")
    if stage == "vendor" and (not parts.get("cix") or parts.get("unofficial")):
        raise ReconstructionError(f"invalid vendor source-target branch: {branch}")
    if stage == "custom" and not parts.get("unofficial"):
        raise ReconstructionError(f"invalid custom source-target branch: {branch}")
    return parts


def rendered_tree_for(repo: Path, branch: str) -> str | None:
    records = rendered_ref_records(repo)
    if branch in records:
        return records[branch].get("tree_id")
    try:
        parts = release_branch_parts(branch)
    except ReconstructionError:
        return None
    target = alias_target_for(branch, parts)
    if target:
        return records.get(target, {}).get("tree_id")
    return None


def alias_target_for(branch: str, parts: dict[str, str]) -> str | None:
    unofficial = parts.get("unofficial")
    if not unofficial or unofficial == "unofficial":
        return None
    release = parts["release"]
    radxa = parts["radxa"]
    cix = parts.get("cix")
    if cix:
        return f"{CACHE_RELEASE_PREFIX}custom/edk2-{release}/cix-{cix}/radxa-{radxa}/unofficial"
    return f"{CACHE_RELEASE_PREFIX}custom/edk2-{release}/radxa-{radxa}/unofficial"


def synthesise_release_entry(repo: Path, branch: str) -> dict[str, Any]:
    parts = release_branch_parts(branch)
    stage = parts["stage"]
    release = parts["release"]
    edk2_ref = edk2_ref_for_release(release)
    radxa = parts["radxa"]
    cix = parts.get("cix")
    unofficial = parts.get("unofficial")

    entry: dict[str, Any] = {
        "description": f"Firmware source target {variant_name(branch)}.",
        "edk2_release": edk2_ref,
        "unofficial_delta": bool(unofficial),
        "radxa_release": radxa,
        "render": {
            "remove_root_gitmodules": True,
            "steps": [],
        },
    }
    if cix:
        entry["cix_release"] = cix

    render = entry["render"]
    radxa_ref = radxa_source_ref(repo, radxa, edk2_ref)
    if stage == "upstream":
        render["base"] = {"ref": radxa_ref}
        render["commit_message"] = f"render: EDK2 {release} with Radxa {radxa} source"
        entry["source_ref"] = radxa_ref
    elif stage == "vendor":
        render["base"] = {"ref": radxa_ref}
        render["commit_message"] = f"render: EDK2 {release} with Radxa {radxa} and CIX {cix} components"
        render["steps"] = [
            {"component": {"path": f"src/cix-v{cix}/tf-a", "ref": f"source/component/cix/{cix}/tf-a"}},
            {"component": {"path": f"src/cix-v{cix}/tee", "ref": f"source/component/cix/{cix}/op-tee"}},
        ]
        entry["source_ref"] = radxa_ref
    else:
        unofficial_ref = f"source/unofficial/{edk2_ref}"
        render["base"] = {"ref": unofficial_ref}
        if cix:
            render["commit_message"] = (
                f"render: firmware source target with EDK2 {release}, Radxa {radxa}, "
                f"CIX {cix} components, and unofficial source"
            )
        else:
            render["commit_message"] = (
                f"render: firmware source target with EDK2 {release}, Radxa {radxa}, "
                "and unofficial source"
            )
        render["steps"] = []
        target = alias_target_for(branch, parts)
        entry["source_ref"] = unofficial_ref
        if target:
            entry["alias_of"] = target

    expected_tree = rendered_tree_for(repo, branch)
    if expected_tree:
        entry["tree_id"] = expected_tree
    return entry


def release_entries(repo: Path) -> dict[str, dict[str, Any]]:
    branches, _aliases = matrix_release_branches(repo)
    return {branch: synthesise_release_entry(repo, branch) for branch in sorted(branches)}


def default_release(repo: Path) -> str:
    entries = release_entries(repo)
    custom = [
        branch
        for branch in entries
        if branch.startswith(f"{CACHE_RELEASE_PREFIX}custom/")
        and "/cix-" in branch
        and re.search(r"/unofficial-[^/]+$", branch)
    ]
    if not custom:
        custom = [branch for branch in entries if branch.startswith(f"{CACHE_RELEASE_PREFIX}custom/") and branch.endswith("/unofficial")]
    if not custom:
        raise ReconstructionError("no default release is configured and no custom unofficial source target can be derived")
    return variant_name(sorted(custom, key=release_branch_sort_key)[-1])


def release_branch_sort_key(branch: str) -> tuple[Any, ...]:
    parts = release_branch_parts(branch)
    return (
        version_key(parts["release"]),
        version_key(parts.get("cix", "")),
        version_key(parts["radxa"]),
        version_key(parts.get("unofficial", "")),
        version_key(parts["stage"]),
    )


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_") or "release"


def ref_exists(repo: Path, ref: str) -> bool:
    return resolve_ref(repo, ref, check=False) is not None


def rev_parse(repo: Path, ref: str) -> str:
    resolved = resolve_ref(repo, ref)
    return git(repo, "rev-parse", f"{resolved}^{{commit}}").stdout.strip()


def tree_id(repo: Path, ref: str) -> str:
    resolved = resolve_ref(repo, ref)
    return git(repo, "rev-parse", f"{resolved}^{{tree}}").stdout.strip()


def ref_type(repo: Path, ref: str) -> str | None:
    resolved = resolve_ref(repo, ref, check=False) or ref
    result = git(repo, "cat-file", "-t", resolved, check=False)
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def show_file(repo: Path, ref: str, path: str, check: bool = True) -> bytes:
    resolved = resolve_ref(repo, ref, check=check)
    if resolved is None:
        return b""
    result = subprocess.run(
        ["git", "-C", str(repo), "show", f"{resolved}:{path}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        raise ReconstructionError(
            f"could not read {path} from {ref}: {result.stderr.decode('utf-8', errors='ignore').strip()}"
        )
    return result.stdout


def release_entry(repo: Path, release: str | None, require: bool = False) -> tuple[str, dict[str, Any]]:
    selected = release or default_release(repo)
    if not selected:
        if require:
            raise ReconstructionError("RELEASE is required and no default release is configured")
        raise ReconstructionError("no release selected")
    entries = release_entries(repo)
    matches = [
        (branch, entry)
        for branch, entry in entries.items()
        if selected in {branch, short_release(branch), variant_name(branch)}
    ]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        variants = "\n".join(f"  - {branch}" for branch, _entry in matches)
        raise ReconstructionError(f"ambiguous firmware source target: {selected}\n{variants}")

    branch = release_to_branch(selected)
    entry = entries.get(branch) or entries.get(short_release(branch))
    if entry is None:
        raise ReconstructionError(
            f"unknown firmware source target: {selected}\n"
            "Use 'make help-source-targets' to list configured source targets."
        )
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


def component_skeleton_tree(repo: Path, components: list[dict[str, str]]) -> str:
    """Return the deterministic tree ID for component refs at configured paths."""
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

    return build(root_entries)


def commit_component_skeleton(repo: Path, components: list[dict[str, str]], message: str) -> str:
    """Create a commit whose tree contains component refs at their configured paths."""

    root_tree = component_skeleton_tree(repo, components)
    return subprocess.run(
        ["git", "-C", str(repo), "commit-tree", root_tree, "-m", message],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout.decode("ascii").strip()


def render_base_tree_commit(repo: Path, base_ref: str) -> str:
    """Return a commit for a generated EDK2 base skeleton without persisting it."""

    record = base_tree_records(repo).get(base_ref)
    if not record:
        raise ReconstructionError(f"generated base cache cannot be derived from {EDK2_REFS_MANIFEST}: {base_ref}")
    components = record.get("components", [])
    if not components:
        raise ReconstructionError(f"generated base cache is missing component metadata: {base_ref}")
    commit = commit_component_skeleton(
        repo,
        components,
        f"base: render EDK2 component skeleton {base_ref.rsplit('/', 1)[-1]}",
    )
    expected_tree = record.get("tree_id")
    actual_tree = tree_id(repo, commit)
    if expected_tree and actual_tree != expected_tree:
        raise ReconstructionError(
            f"generated base cache tree differs for {base_ref}: {actual_tree} != {expected_tree}"
        )
    return commit


def resolve_ref_or_generated_cache(repo: Path, ref: str) -> str:
    resolved = resolve_ref(repo, ref, check=False)
    if resolved:
        return resolved
    if ref.startswith(CACHE_BASE_EDK2_PREFIX):
        return render_base_tree_commit(repo, ref)
    raise ReconstructionError(f"ref is unavailable locally and is not a generated cache ref: {ref}")


def delta_metadata(repo: Path, base_ref: str, target_ref: str, kind: str, name: str) -> dict[str, Any]:
    gitlinks = []
    resolved_target = resolve_ref(repo, target_ref)
    for line in git(repo, "ls-tree", "-r", resolved_target).stdout.splitlines():
        if line.startswith("160000 "):
            meta, path = line.split("\t", 1)
            _mode, _kind, oid = meta.split()
            gitlinks.append({"path": path, "object_id": oid})
    gitmodules = []
    for line in git(repo, "ls-tree", "-r", resolved_target).stdout.splitlines():
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
    metadata_base_ref: str | None = None,
) -> str:
    """Create a branch containing metadata.json and delta.patch for base..target."""

    if not ref_exists(repo, base_ref):
        raise ReconstructionError(f"base ref is unavailable: {base_ref}")
    if not ref_exists(repo, target_ref):
        raise ReconstructionError(f"target ref is unavailable: {target_ref}")
    old = rev_parse(repo, artefact_ref) if ref_exists(repo, artefact_ref) else None
    if old and not allow_replace:
        raise ReconstructionError(f"delta artefact ref already exists: {artefact_ref}")
    resolved_base = resolve_ref(repo, base_ref)
    resolved_target = resolve_ref(repo, target_ref)
    diff_result = subprocess.run(
        ["git", "-C", str(repo), "diff", "--binary", "--full-index", f"{resolved_base}..{resolved_target}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if diff_result.returncode not in {0, 1}:
        raise ReconstructionError(diff_result.stderr.decode("utf-8", errors="ignore").strip())
    diff = diff_result.stdout
    metadata = delta_metadata(repo, base_ref, target_ref, kind, name)
    if metadata_base_ref:
        metadata["base_ref"] = metadata_base_ref
    display_base_ref = metadata_base_ref or base_ref
    files = {
        "README.md": (
            f"# Delta Artefact: {artefact_ref}\n\n"
            f"Kind: `{kind}`\n\n"
            f"Base: `{display_base_ref}`\n\n"
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
    for path in sorted((repo / "config").glob("refs-*.json")):
        for record in ref_manifest_records(repo, path.name):
            record = dict(record)
            record.setdefault("manifest", str(path.relative_to(repo)))
            records.append(record)
    return records


def update_ref_record(repo: Path, manifest_name: str, ref: str, updates: dict[str, Any]) -> None:
    """Update or create a config refs-* record for a ref moved by an explicit workflow."""

    path = repo / "config" / manifest_name
    if path.exists():
        data = load_json(repo, f"config/{manifest_name}")
    else:
        data = {"refs": []}
    records = data.setdefault("refs", [])
    record = None
    for candidate in records:
        candidate_refs = candidate.get("refs")
        if candidate.get("ref") == ref:
            record = candidate
            break
        if isinstance(candidate_refs, list) and ref in candidate_refs:
            remaining_refs = [candidate_ref for candidate_ref in candidate_refs if candidate_ref != ref]
            if len(remaining_refs) > 1:
                candidate["refs"] = remaining_refs
            elif remaining_refs:
                candidate.pop("refs", None)
                candidate["ref"] = remaining_refs[0]
            else:
                records.remove(candidate)

            record = {
                key: deepcopy(value)
                for key, value in candidate.items()
                if key not in {"ref", "refs"}
            }
            record["ref"] = ref
            records.append(record)
            break
    if record is None:
        record = {"ref": ref}
        records.append(record)
    for key, value in updates.items():
        if value is None:
            record.pop(key, None)
        else:
            record[key] = value
    data["refs"] = sorted(records, key=manifest_record_sort_key)
    write_json(path, data)
    clear_metadata_caches()


def refresh_ref_record(repo: Path, manifest_name: str, ref: str, extra: dict[str, Any] | None = None) -> None:
    updates: dict[str, Any] = {
        "object_id": rev_parse(repo, ref),
        "tree_id": tree_id(repo, ref),
    }
    if extra:
        updates.update(extra)
    update_ref_record(repo, manifest_name, ref, updates)


def refresh_release_tree(repo: Path, branch: str) -> None:
    if branch not in release_entries(repo):
        raise ReconstructionError(f"cannot update source-target manifest for unknown source/cache/release branch: {branch}")
    parts = release_branch_parts(branch)
    manifest_branch = alias_target_for(branch, parts) or branch
    update_ref_record(
        repo,
        VARIANT_CACHE_MANIFEST,
        manifest_branch,
        {
            "object_id": None,
            "tree_id": tree_id(repo, branch),
        },
    )


def immutable_records(repo: Path) -> list[dict[str, Any]]:
    return [r for r in load_ref_records(repo) if r.get("immutable", False)]


def generated_ref_record(record: dict[str, Any]) -> bool:
    """Return True for refs that can be recreated from manifests."""

    record_type = str(record.get("type", ""))
    return record_type.startswith("rendered-") or record_type == "component-skeleton"


def is_immutable_namespace(ref: str) -> bool:
    if ref.startswith("source/base/"):
        return True
    if ref.startswith("source/vendor/"):
        return True
    if ref.startswith("source/port/"):
        return True
    if ref.startswith("source/component/cix/"):
        return True
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
                problems.append(f"{ref}: immutable namespace ref is not recorded in config/refs-*.json")
    for record in records:
        ref = record.get("ref")
        if not ref or (wanted and ref not in wanted):
            continue
        if not ref_exists(repo, ref):
            # Generated refs may be omitted by a pruned clone; any copy that is
            # present is still checked below against its recorded tree.
            if generated_ref_record(record):
                continue
            if record.get("optional"):
                continue
            problems.append(f"{ref}: missing locally")
            continue
        expected_oid = record.get("object_id")
        expected_tree = record.get("tree_id")
        actual_oid = rev_parse(repo, ref)
        actual_tree = tree_id(repo, ref)
        is_generated = generated_ref_record(record)
        if expected_oid and actual_oid != expected_oid and not allow_manifest_update and not is_generated:
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
    except KeyboardInterrupt:
        die("interrupted by user", 130)
    except Exception as exc:
        if truthy(os.environ.get("DEBUG")):
            traceback.print_exc()
            raise SystemExit(1)
        die(f"internal error: {exc}\nRe-run with DEBUG=1 to show the Python traceback.", 1)
