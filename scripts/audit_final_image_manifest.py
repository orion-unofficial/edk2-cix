#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import shlex
import sys
from typing import Any


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
DEFAULT_BASELINE_FILE = REPO_ROOT / "validation" / "final-image-manifests.json"
DEFAULT_PROFILE = "upstream-1.2.1-bookworm"

FV_SIZE_RE = re.compile(r"^EFI_FV_(TOTAL|TAKEN)_SIZE = (0x[0-9a-fA-F]+)$")
FV_ENTRY_RE = re.compile(r"^(0x[0-9a-fA-F]+)\s+([0-9A-Fa-f-]{36})$")
BUILD_PATH_RE = re.compile(
    r"(?:^|[/\\\\])Build[/\\\\](?P<board>[^/\\\\]+)[/\\\\](?P<target>[^/\\\\]+)[/\\\\](?P<suffix>.+)$"
)
SRC_PATH_RE = re.compile(r"(?:^|[/\\\\])src[/\\\\](?P<suffix>.+)$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Audit the final BL33 firmware image composition against a checked baseline."
        )
    )
    parser.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument("--board", default="O6")
    parser.add_argument("--target", default="RELEASE_GCC5")
    parser.add_argument("--profile", default=DEFAULT_PROFILE)
    parser.add_argument("--baseline-file", type=pathlib.Path, default=DEFAULT_BASELINE_FILE)
    parser.add_argument("--report-json", type=pathlib.Path)
    parser.add_argument("--emit-baseline", type=pathlib.Path)
    parser.add_argument("--emit-profile-name")
    return parser.parse_args()


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_build_dir(args: argparse.Namespace) -> pathlib.Path:
    if args.build_dir:
        return args.build_dir.resolve()
    return args.repo_root.resolve() / "src" / "Build" / args.board / args.target


def load_profiles(path: pathlib.Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    profiles = data.get("profiles")
    if not isinstance(profiles, dict):
        raise ValueError(f"{path} does not contain a top-level 'profiles' mapping")
    return profiles


def load_baseline_payload(path: pathlib.Path) -> dict[str, Any]:
    if not path.is_file():
        return {"profiles": {}}
    data = json.loads(path.read_text(encoding="utf-8"))
    profiles = data.get("profiles")
    if not isinstance(profiles, dict):
        raise ValueError(f"{path} does not contain a top-level 'profiles' mapping")
    return data


def resolve_profile(
    profiles: dict[str, Any],
    profile_name: str,
    board: str,
    target: str,
) -> tuple[dict[str, Any], dict[str, Any]]:
    profile = profiles.get(profile_name)
    if not isinstance(profile, dict):
        raise KeyError(profile_name)

    board_profiles = profile.get("boards")
    if not isinstance(board_profiles, dict):
        raise ValueError(f"Profile '{profile_name}' does not contain a 'boards' mapping")

    board_profile = board_profiles.get(board)
    if not isinstance(board_profile, dict):
        available = ", ".join(sorted(board_profiles))
        raise ValueError(
            f"Profile '{profile_name}' does not define board '{board}' "
            f"(available: {available})"
        )

    recorded_target = board_profile.get("target") or profile.get("target")
    if recorded_target and recorded_target != target:
        raise ValueError(
            f"Profile '{profile_name}' for board '{board}' is recorded for target "
            f"'{recorded_target}', not '{target}'"
        )

    return profile, board_profile


def normalize_path(value: str, repo_root: pathlib.Path) -> str:
    normalized = value.replace("\\", "/")
    match = BUILD_PATH_RE.search(normalized)
    if match:
        return (
            f"Build/{match.group('board')}/{match.group('target')}/"
            f"{match.group('suffix')}"
        )

    src_match = SRC_PATH_RE.search(normalized)
    if src_match:
        return f"src/{src_match.group('suffix')}"

    repo_root_str = repo_root.as_posix().rstrip("/")
    if normalized.startswith(repo_root_str + "/"):
        return normalized[len(repo_root_str) + 1 :]

    return pathlib.PurePosixPath(normalized).name


def normalize_path_like(value: str | None, repo_root: pathlib.Path) -> str | None:
    if value is None:
        return None
    if "/" not in value and "\\" not in value:
        return value
    return normalize_path(value, repo_root)


def parse_guid_xref(path: pathlib.Path, repo_root: pathlib.Path) -> dict[str, str]:
    mapping: dict[str, str] = {}
    if not path.is_file():
        return mapping
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        parts = stripped.split(maxsplit=1)
        if len(parts) != 2:
            continue
        guid, name = parts
        mapping[guid.upper()] = normalize_path_like(name, repo_root) or name
    return mapping


def parse_fv_entries(path: pathlib.Path, guid_names: dict[str, str]) -> dict[str, Any]:
    fv_name = path.name.replace(".Fv.txt", "")
    entries: list[dict[str, Any]] = []
    total_size = None
    taken_size = None
    for line in path.read_text(encoding="utf-8").splitlines():
        size_match = FV_SIZE_RE.match(line)
        if size_match:
            which, value = size_match.groups()
            if which == "TOTAL":
                total_size = value.lower()
            else:
                taken_size = value.lower()
            continue

        entry_match = FV_ENTRY_RE.match(line)
        if not entry_match:
            continue
        offset, guid = entry_match.groups()
        normalized_guid = guid.upper()
        entries.append(
            {
                "offset": offset.lower(),
                "guid": normalized_guid,
                "module": guid_names.get(normalized_guid),
            }
        )

    fv_file = path.with_suffix("")
    fv_manifest: dict[str, Any] = {
        "name": fv_name,
        "total_size": total_size,
        "taken_size": taken_size,
        "exists": fv_file.is_file(),
        "entries": entries,
    }
    if fv_file.is_file():
        fv_manifest["size"] = fv_file.stat().st_size
        fv_manifest["sha256"] = sha256_file(fv_file)
    return fv_manifest


def parse_command(path: pathlib.Path) -> list[str]:
    return shlex.split(path.read_text(encoding="utf-8").strip())


def decode_ui_string(path: pathlib.Path) -> str | None:
    if not path.is_file():
        return None
    try:
        decoded = path.read_bytes().decode("utf-16le", errors="ignore")
    except OSError:
        return None
    candidates = re.findall(r"[A-Za-z0-9_. -]{3,}", decoded)
    if not candidates:
        return None
    return max(candidates, key=len).strip()


def infer_ui_name(module_dir: pathlib.Path, ui_section: pathlib.Path | None) -> str | None:
    if ui_section is not None:
        decoded = decode_ui_string(ui_section)
        if decoded:
            return decoded

    suffix = module_dir.name[36:] if len(module_dir.name) > 36 else ""
    return suffix or None


def parse_gensec_metadata(
    section_file: pathlib.Path,
    text_path: pathlib.Path,
    repo_root: pathlib.Path,
) -> dict[str, Any]:
    tokens = parse_command(text_path)
    metadata: dict[str, Any] = {
        "path": section_file.name,
        "size": section_file.stat().st_size,
        "sha256": sha256_file(section_file),
    }
    index = 0
    source_path = None
    attributes: list[str] = []
    while index < len(tokens):
        token = tokens[index]
        if token == "-s" and index + 1 < len(tokens):
            metadata["type"] = tokens[index + 1]
            index += 2
            continue
        if token == "-g" and index + 1 < len(tokens):
            metadata["guid"] = tokens[index + 1].upper()
            index += 2
            continue
        if token == "-r" and index + 1 < len(tokens):
            attributes.append(tokens[index + 1])
            index += 2
            continue
        if token == "-o" and index + 1 < len(tokens):
            index += 2
            continue
        if token.startswith("-"):
            index += 1
            continue
        source_path = token
        index += 1

    if attributes:
        metadata["attributes"] = attributes
    if source_path:
        metadata["source"] = normalize_path(source_path, repo_root)
    return metadata


def infer_section_type(section_file: pathlib.Path) -> str:
    suffix = section_file.suffix.lower()
    if suffix == ".ui":
        return "EFI_SECTION_USER_INTERFACE"
    if suffix == ".raw":
        return "EFI_SECTION_RAW"
    if suffix == ".pe32":
        return "EFI_SECTION_PE32"
    if suffix == ".dpx":
        return "EFI_SECTION_DXE_DEPEX"
    if suffix == ".guided":
        return "EFI_SECTION_GUID_DEFINED"
    return "UNKNOWN"


def parse_section_entry(
    module_dir: pathlib.Path,
    section_path_text: str,
    repo_root: pathlib.Path,
) -> dict[str, Any]:
    section_path = pathlib.Path(section_path_text)
    section_file = module_dir / section_path.name
    section_text = module_dir / f"{section_path.name}.txt"
    if not section_file.is_file():
        raise FileNotFoundError(section_file)

    if section_text.is_file():
        metadata = parse_gensec_metadata(section_file, section_text, repo_root)
    else:
        metadata = {
            "path": section_file.name,
            "type": infer_section_type(section_file),
            "size": section_file.stat().st_size,
            "sha256": sha256_file(section_file),
        }

    if metadata.get("type") == "EFI_SECTION_USER_INTERFACE":
        ui_name = decode_ui_string(section_file)
        if ui_name:
            metadata["ui_name"] = ui_name
    return metadata


def parse_ffs_manifest(
    module_dir: pathlib.Path,
    repo_root: pathlib.Path,
    guid_names: dict[str, str],
    locations: dict[str, list[dict[str, str]]],
) -> dict[str, Any]:
    manifest_path = next(module_dir.glob("*.ffs.txt"))
    ffs_path = manifest_path.with_suffix("")
    tokens = parse_command(manifest_path)

    file_type = None
    guid = None
    section_inputs: list[str] = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token == "-t" and index + 1 < len(tokens):
            file_type = tokens[index + 1]
            index += 2
            continue
        if token == "-g" and index + 1 < len(tokens):
            guid = tokens[index + 1].upper()
            index += 2
            continue
        if token in {"-i", "-oi"} and index + 1 < len(tokens):
            section_inputs.append(tokens[index + 1])
            index += 2
            continue
        index += 1

    if not guid or not file_type:
        raise ValueError(f"Could not parse GenFfs metadata from {manifest_path}")

    ui_section = None
    for section_input in section_inputs:
        candidate = module_dir / pathlib.Path(section_input).name
        if candidate.suffix.lower() == ".ui":
            ui_section = candidate
            break

    sections = [
        parse_section_entry(module_dir, section_input, repo_root)
        for section_input in section_inputs
    ]

    return {
        "guid": guid,
        "module": guid_names.get(guid),
        "directory": module_dir.name,
        "ui_name": infer_ui_name(module_dir, ui_section),
        "file_type": file_type,
        "size": ffs_path.stat().st_size if ffs_path.is_file() else None,
        "sha256": sha256_file(ffs_path) if ffs_path.is_file() else None,
        "locations": locations.get(guid, []),
        "sections": sections,
    }


def gather_manifest(
    repo_root: pathlib.Path,
    build_dir: pathlib.Path,
    board: str,
    target: str,
) -> dict[str, Any]:
    fv_dir = build_dir / "FV"
    guid_names = parse_guid_xref(fv_dir / "Guid.xref", repo_root)

    fvs: dict[str, Any] = {}
    locations: dict[str, list[dict[str, str]]] = {}
    for fv_text in sorted(fv_dir.glob("*.Fv.txt")):
        fv_manifest = parse_fv_entries(fv_text, guid_names)
        fvs[fv_manifest["name"]] = fv_manifest
        for entry in fv_manifest["entries"]:
            locations.setdefault(entry["guid"], []).append(
                {"fv": fv_manifest["name"], "offset": entry["offset"]}
            )

    modules = []
    ffs_root = fv_dir / "Ffs"
    for module_dir in sorted(path for path in ffs_root.iterdir() if path.is_dir()):
        if not any(module_dir.glob("*.ffs.txt")):
            continue
        modules.append(parse_ffs_manifest(module_dir, repo_root, guid_names, locations))

    def module_sort_key(entry: dict[str, Any]) -> tuple[str, str]:
        first_location = entry.get("locations") or []
        if first_location:
            return (first_location[0]["offset"], entry["guid"])
        return ("0xffffffff", entry["guid"])

    modules.sort(key=module_sort_key)

    fd_path = fv_dir / "SKY1_BL33_UEFI.fd"
    return {
        "fd": {
            "path": "FV/SKY1_BL33_UEFI.fd",
            "size": fd_path.stat().st_size if fd_path.is_file() else None,
            "sha256": sha256_file(fd_path) if fd_path.is_file() else None,
        },
        "fvs": fvs,
        "modules": modules,
        "board": board,
        "target": target,
    }


def compare_manifests(
    expected: dict[str, Any],
    actual: dict[str, Any],
    repo_root: pathlib.Path = REPO_ROOT,
) -> list[str]:
    def normalize_section(section: dict[str, Any]) -> dict[str, Any]:
        normalized = {
            key: value
            for key, value in section.items()
            if key not in {"sha256", "source"}
        }
        if "path" in normalized:
            normalized["path"] = normalize_path_like(normalized["path"], repo_root)
        return normalized

    def normalize_fv_entry(entry: dict[str, Any]) -> dict[str, Any]:
        normalized = dict(entry)
        normalized["module"] = normalize_path_like(normalized.get("module"), repo_root)
        return normalized

    def normalize_module(module: dict[str, Any]) -> dict[str, Any]:
        normalized = {
            key: value
            for key, value in module.items()
            if key not in {"sha256", "sections"}
        }
        normalized["module"] = normalize_path_like(normalized.get("module"), repo_root)
        normalized["sections"] = [normalize_section(section) for section in module.get("sections", [])]
        return normalized

    def normalize_fv(fv: dict[str, Any]) -> dict[str, Any]:
        normalized = {
            key: value
            for key, value in fv.items()
            if key != "sha256"
        }
        normalized["entries"] = [
            normalize_fv_entry(entry)
            for entry in fv.get("entries", [])
        ]
        return normalized

    expected_fd = {
        key: value
        for key, value in expected.get("fd", {}).items()
        if key != "sha256"
    }
    actual_fd = {
        key: value
        for key, value in actual.get("fd", {}).items()
        if key != "sha256"
    }
    mismatches: list[str] = []
    if expected_fd != actual_fd:
        mismatches.append("Final SKY1_BL33_UEFI.fd metadata changed")

    expected_fvs = {
        name: normalize_fv(entry)
        for name, entry in expected.get("fvs", {}).items()
    }
    actual_fvs = {
        name: normalize_fv(entry)
        for name, entry in actual.get("fvs", {}).items()
    }
    if expected_fvs != actual_fvs:
        missing = sorted(set(expected_fvs) - set(actual_fvs))
        unexpected = sorted(set(actual_fvs) - set(expected_fvs))
        changed = sorted(
            name for name in set(expected_fvs) & set(actual_fvs) if expected_fvs[name] != actual_fvs[name]
        )
        if missing:
            mismatches.append(f"Missing firmware volumes: {', '.join(missing)}")
        if unexpected:
            mismatches.append(f"Unexpected firmware volumes: {', '.join(unexpected)}")
        if changed:
            mismatches.append(f"Changed firmware volumes: {', '.join(changed)}")

    expected_modules = {
        entry["guid"]: normalize_module(entry)
        for entry in expected.get("modules", [])
    }
    actual_modules = {
        entry["guid"]: normalize_module(entry)
        for entry in actual.get("modules", [])
    }
    if expected_modules != actual_modules:
        missing = sorted(set(expected_modules) - set(actual_modules))
        unexpected = sorted(set(actual_modules) - set(expected_modules))
        changed = sorted(
            guid
            for guid in set(expected_modules) & set(actual_modules)
            if expected_modules[guid] != actual_modules[guid]
        )
        if missing:
            mismatches.append(f"Missing FFS modules: {', '.join(missing)}")
        if unexpected:
            mismatches.append(f"Unexpected FFS modules: {', '.join(unexpected)}")
        if changed:
            mismatches.append(f"Changed FFS modules: {', '.join(changed)}")

    return mismatches


def emit_baseline(
    path: pathlib.Path,
    profile_name: str,
    board: str,
    target: str,
    manifest: dict[str, Any],
) -> None:
    payload = load_baseline_payload(path)
    profiles = payload.setdefault("profiles", {})
    profile = profiles.setdefault(
        profile_name,
        {
            "description": (
                "Generated from deterministic upstream replay outputs for final "
                "firmware image composition checks."
            ),
            "boards": {},
        },
    )
    boards = profile.setdefault("boards", {})
    boards[board] = {
        "target": target,
        "manifest": manifest,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    build_dir = resolve_build_dir(args)
    manifest = gather_manifest(repo_root, build_dir, args.board, args.target)

    if args.emit_baseline:
        profile_name = args.emit_profile_name or args.profile
        emit_baseline(args.emit_baseline, profile_name, args.board, args.target, manifest)
        print(f"Generated manifest baseline: {args.emit_baseline}")
        return 0

    try:
        profiles = load_profiles(args.baseline_file.resolve())
        profile_meta, board_profile = resolve_profile(profiles, args.profile, args.board, args.target)
    except KeyError:
        print(
            f"Unknown manifest-audit profile '{args.profile}' in {args.baseline_file}",
            file=sys.stderr,
        )
        return 2
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    expected_manifest = board_profile.get("manifest")
    if not isinstance(expected_manifest, dict):
        print(
            f"Profile '{args.profile}' board '{args.board}' is missing a manifest baseline",
            file=sys.stderr,
        )
        return 2

    mismatches = compare_manifests(expected_manifest, manifest, repo_root=repo_root)
    report = {
        "profile": args.profile,
        "description": profile_meta.get("description"),
        "board": args.board,
        "target": args.target,
        "build_dir": str(build_dir),
        "status": "match" if not mismatches else "mismatch",
        "mismatches": mismatches,
        "manifest": manifest,
    }

    print(f"Manifest profile: {args.profile}")
    print(f"Build directory: {build_dir}")
    if mismatches:
        print("Final-image manifest audit: mismatch")
        for mismatch in mismatches:
            print(f"  - {mismatch}")
    else:
        print("Final-image manifest audit: match")

    if args.report_json:
        args.report_json.parent.mkdir(parents=True, exist_ok=True)
        args.report_json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"Manifest report: {args.report_json}")

    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
