#!/usr/bin/env python3

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import sys
from typing import Any


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
DEFAULT_PROFILE_FILE = REPO_ROOT / "validation" / "expected-hashes.json"
DEFAULT_PROFILE = "upstream-1.2.1-bookworm"
DEFAULT_PE_FILES = (
    "AARCH64/Shell.efi",
    "AARCH64/VariableInfo.efi",
    "AARCH64/EnrollFromDefaultKeysApp.efi",
)
DEFAULT_OPTIONAL_FILES = (
    "FV/SKY1_BL33_UEFI.fd",
    "Firmwares/bootloader3.img",
)
SECTION_RE = re.compile(r"^\s*\d+\s+(\S+)\s+([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+(\S+)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate built firmware artefacts against a stored baseline and "
            "capture structural metadata for later comparison."
        )
    )
    parser.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument("--board", default="O6")
    parser.add_argument("--target", default="RELEASE_GCC5")
    parser.add_argument("--profile-file", type=pathlib.Path, default=DEFAULT_PROFILE_FILE)
    parser.add_argument("--profile", default=DEFAULT_PROFILE)
    parser.add_argument("--report-json", type=pathlib.Path)
    parser.add_argument("--emit-profile", type=pathlib.Path)
    parser.add_argument("--emit-profile-name")
    parser.add_argument("--strict", action="store_true")
    return parser.parse_args()


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_profile_data(profile_file: pathlib.Path) -> dict[str, Any]:
    data = json.loads(profile_file.read_text(encoding="utf-8"))
    return data


def load_profiles(profile_file: pathlib.Path) -> dict[str, Any]:
    data = load_profile_data(profile_file)
    return data.get("profiles", {})


def load_profile_aliases(profile_file: pathlib.Path) -> dict[str, str]:
    data = load_profile_data(profile_file)
    aliases = data.get("profile_aliases", {})
    if not isinstance(aliases, dict):
        return {}
    return {str(key): str(value) for key, value in aliases.items()}


def resolve_profile(
    profiles: dict[str, Any],
    profile_aliases: dict[str, str],
    profile_name: str,
    board: str,
    target: str,
) -> tuple[str, dict[str, Any], dict[str, Any]]:
    resolved_profile_name = profile_aliases.get(profile_name, profile_name)
    profile = profiles.get(resolved_profile_name)
    if profile is None:
        raise KeyError(resolved_profile_name)

    board_profiles = profile.get("boards")
    if not isinstance(board_profiles, dict):
        legacy_board = profile.get("board")
        if legacy_board and legacy_board != board:
            raise ValueError(
                f"Profile '{resolved_profile_name}' is recorded for board '{legacy_board}', not '{board}'"
            )
        legacy_target = profile.get("target")
        if legacy_target and legacy_target != target:
            raise ValueError(
                f"Profile '{resolved_profile_name}' is recorded for target '{legacy_target}', not '{target}'"
            )
        return resolved_profile_name, profile, profile

    board_profile = board_profiles.get(board)
    if board_profile is None:
        available = ", ".join(sorted(board_profiles))
        raise ValueError(
            f"Profile '{resolved_profile_name}' does not define board '{board}'"
            f" (available: {available})"
        )

    profile_target = board_profile.get("target") or profile.get("target")
    if profile_target and profile_target != target:
        raise ValueError(
            f"Profile '{resolved_profile_name}' for board '{board}' is recorded for target "
            f"'{profile_target}', not '{target}'"
        )

    return resolved_profile_name, profile, board_profile


def resolve_build_dir(args: argparse.Namespace) -> pathlib.Path:
    if args.build_dir:
        return args.build_dir.resolve()
    return args.repo_root.resolve() / "src" / "Build" / args.board / args.target


def parse_build_options(path: pathlib.Path) -> dict[str, Any]:
    result: dict[str, Any] = {}
    if not path.is_file():
        return result
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("gCommandLineDefines: "):
            payload = line.partition(": ")[2]
            result["gCommandLineDefines"] = ast.literal_eval(payload)
        elif line.startswith("Active Platform: "):
            result["active_platform"] = line.partition(": ")[2].strip()
        elif line.startswith("Flash Image Definition: "):
            result["flash_definition"] = line.partition(": ")[2].strip()
    return result


def find_objdump() -> str | None:
    for candidate in ("aarch64-linux-gnu-objdump", "llvm-objdump", "objdump", "gobjdump"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    return None


def parse_pe_sections(path: pathlib.Path, objdump: str | None) -> list[dict[str, Any]] | None:
    if not objdump or not path.is_file():
        return None
    result = subprocess.run(
        [objdump, "-h", str(path)],
        check=True,
        capture_output=True,
        text=True,
    )
    sections: list[dict[str, Any]] = []
    for line in result.stdout.splitlines():
        match = SECTION_RE.match(line)
        if not match:
            continue
        name, size_hex, section_type = match.groups()
        sections.append(
            {
                "name": name,
                "size": int(size_hex, 16),
                "type": section_type,
            }
        )
    return sections


def find_fiptool(repo_root: pathlib.Path) -> str | None:
    for candidate in (
        repo_root / "src" / "tools" / "arm-trusted-firmware-fiptool" / "build" / "aarch64" / "fiptool",
        repo_root / "src" / "tools" / "arm-trusted-firmware-fiptool" / "build" / "x86_64" / "fiptool",
    ):
        if not candidate.is_file():
            continue
        try:
            subprocess.run(
                [str(candidate), "--help"],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except OSError:
            continue
        return str(candidate)

    fallback = shutil.which("fiptool")
    if not fallback:
        return None
    try:
        subprocess.run(
            [fallback, "--help"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except OSError:
        return None
    return fallback


def gather_fip_info(path: pathlib.Path, fiptool: str | None) -> list[str] | None:
    if not fiptool or not path.is_file():
        return None
    result = subprocess.run(
        [fiptool, "info", str(path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return [line.rstrip() for line in result.stdout.splitlines() if line.strip()]


def compare_expected(actual: dict[str, Any], expected: dict[str, Any]) -> tuple[bool, list[str]]:
    mismatches: list[str] = []
    for key in ("size", "sha256"):
        if key in expected and actual.get(key) != expected[key]:
            mismatches.append(f"{key}: expected {expected[key]}, got {actual.get(key)}")
    if "min_size" in expected and actual.get("size", 0) < expected["min_size"]:
        mismatches.append(f"min_size: expected at least {expected['min_size']}, got {actual.get('size')}")
    if "max_size" in expected and actual.get("size", 0) > expected["max_size"]:
        mismatches.append(f"max_size: expected at most {expected['max_size']}, got {actual.get('size')}")
    return (not mismatches, mismatches)


def suffix_matches(actual: str | None, expected_suffix: str | None) -> bool | None:
    if expected_suffix is None:
        return None
    if actual is None:
        return False
    return actual.endswith(expected_suffix)


def exact_matches(actual: str | None, expected_exact: str | None) -> bool | None:
    if expected_exact is None:
        return None
    if actual is None:
        return False
    return actual == expected_exact


def compare_path_field(
    actual: str | None,
    field_name: str,
    expected_exact: str | None,
    expected_suffix: str | None,
) -> list[str]:
    exact_match = exact_matches(actual, expected_exact)
    if exact_match is not None:
        if exact_match:
            return []
        return [f"{field_name}: expected {expected_exact}, got {actual}"]

    suffix_match = suffix_matches(actual, expected_suffix)
    if suffix_match is False:
        return [f"{field_name} suffix mismatch: expected suffix {expected_suffix}, got {actual}"]
    return []


def should_record_exact_path(value: str) -> bool:
    return not value.startswith("/") and re.match(r"^[A-Za-z]:[\\/]", value) is None


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    build_dir = resolve_build_dir(args)
    profile_file = args.profile_file.resolve()
    profiles = load_profiles(profile_file)
    profile_aliases = load_profile_aliases(profile_file)
    try:
        resolved_profile_name, profile_meta, profile = resolve_profile(
            profiles,
            profile_aliases,
            args.profile,
            args.board,
            args.target,
        )
    except KeyError:
        print(
            f"Unknown profile '{args.profile}' in {args.profile_file}",
            file=sys.stderr,
        )
        return 2
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    artefact_specs = dict(profile.get("artefacts", {}))
    objdump = find_objdump()
    fiptool = find_fiptool(repo_root)

    report: dict[str, Any] = {
        "requested_profile": args.profile,
        "profile": resolved_profile_name,
        "description": profile_meta.get("description"),
        "board": args.board,
        "target": args.target,
        "repo_root": str(repo_root),
        "build_dir": str(build_dir),
        "artefacts": {},
        "build_options": {},
        "pe_sections": {},
        "fip_info": {},
        "summary": {},
    }

    matched = 0
    mismatched = 0
    missing = 0

    for label, spec in artefact_specs.items():
        relative_path = pathlib.Path(spec["path"])
        actual_path = build_dir / relative_path
        entry: dict[str, Any] = {
            "path": str(relative_path),
            "exists": actual_path.is_file(),
        }
        if actual_path.is_file():
            entry["size"] = actual_path.stat().st_size
            entry["sha256"] = sha256_file(actual_path)
            ok, reasons = compare_expected(entry, spec)
            entry["status"] = "match" if ok else "mismatch"
            if reasons:
                entry["mismatches"] = reasons
            if ok:
                matched += 1
            else:
                mismatched += 1
        else:
            entry["status"] = "missing"
            missing += 1
        report["artefacts"][label] = entry

    build_options_path = build_dir / "BuildOptions"
    build_options_actual = parse_build_options(build_options_path)
    build_options_expected = profile.get("build_options", {})
    build_options_report: dict[str, Any] = {
        "path": "BuildOptions",
        "exists": build_options_path.is_file(),
        "actual": build_options_actual,
        "status": "skipped",
        "mismatches": [],
    }
    if build_options_actual:
        build_options_report["status"] = "match"
        expected_defines = build_options_expected.get("gCommandLineDefines", {})
        actual_defines = build_options_actual.get("gCommandLineDefines", {})
        define_mismatches = []
        for key, expected_value in expected_defines.items():
            actual_value = actual_defines.get(key)
            if actual_value != expected_value:
                define_mismatches.append(
                    f"gCommandLineDefines[{key}]: expected {expected_value}, got {actual_value}"
                )
        build_options_report["mismatches"].extend(
            compare_path_field(
                build_options_actual.get("active_platform"),
                "active_platform",
                build_options_expected.get("active_platform"),
                build_options_expected.get("active_platform_suffix"),
            )
        )
        build_options_report["mismatches"].extend(
            compare_path_field(
                build_options_actual.get("flash_definition"),
                "flash_definition",
                build_options_expected.get("flash_definition"),
                build_options_expected.get("flash_definition_suffix"),
            )
        )
        build_options_report["mismatches"].extend(define_mismatches)
        if build_options_report["mismatches"]:
            build_options_report["status"] = "mismatch"
    report["build_options"] = build_options_report

    pe_expected = profile.get("pe_sections", {})
    for relative_name in sorted(set(DEFAULT_PE_FILES) | set(pe_expected)):
        path = build_dir / relative_name
        sections = parse_pe_sections(path, objdump)
        entry: dict[str, Any] = {
            "path": relative_name,
            "exists": path.is_file(),
            "objdump": objdump,
            "status": "skipped" if not sections else "match",
            "sections": sections,
            "mismatches": [],
        }
        expected_sections = pe_expected.get(relative_name)
        if sections is None and path.is_file():
            entry["status"] = "skipped"
            entry["mismatches"].append("No objdump tool available")
        elif sections is None:
            entry["status"] = "missing"
        elif expected_sections is not None:
            actual_min = [{"name": section["name"], "size": section["size"]} for section in sections]
            if actual_min != expected_sections:
                entry["status"] = "mismatch"
                entry["mismatches"].append(
                    f"Expected sections {expected_sections}, got {actual_min}"
                )
        report["pe_sections"][relative_name] = entry

    for relative_name in DEFAULT_OPTIONAL_FILES:
        path = build_dir / relative_name
        if relative_name.endswith("bootloader3.img"):
            report["fip_info"][relative_name] = {
                "path": relative_name,
                "exists": path.is_file(),
                "tool": fiptool,
                "lines": gather_fip_info(path, fiptool),
            }
        else:
            report["fip_info"][relative_name] = {
                "path": relative_name,
                "exists": path.is_file(),
                "sha256": sha256_file(path) if path.is_file() else None,
                "size": path.stat().st_size if path.is_file() else None,
            }

    build_option_status = report["build_options"]["status"]
    section_mismatches = sum(
        1
        for entry in report["pe_sections"].values()
        if entry["status"] == "mismatch"
    )
    report["summary"] = {
        "matched_hashes": matched,
        "mismatched_hashes": mismatched,
        "missing_hashes": missing,
        "build_options_status": build_option_status,
        "section_mismatches": section_mismatches,
        "objdump": objdump,
        "fiptool": fiptool,
    }

    if args.emit_profile:
        generated_profile_name = args.emit_profile_name or args.profile
        generated_profile: dict[str, Any] = {
            "profiles": {
                generated_profile_name: {
                    "description": f"Generated from {build_dir}",
                    "boards": {
                        args.board: {
                            "target": args.target,
                            "artefacts": {},
                            "build_options": {},
                            "pe_sections": {},
                        }
                    },
                }
            }
        }
        profile_entry = generated_profile["profiles"][generated_profile_name]["boards"][args.board]
        for label, entry in report["artefacts"].items():
            if entry.get("exists"):
                profile_entry["artefacts"][label] = {
                    "path": entry["path"],
                    "size": entry.get("size"),
                    "sha256": entry.get("sha256"),
                }
        if build_options_actual:
            profile_entry["build_options"] = {
                "gCommandLineDefines": build_options_actual.get("gCommandLineDefines", {}),
            }
            if build_options_actual.get("active_platform"):
                key = "active_platform" if should_record_exact_path(build_options_actual["active_platform"]) else "active_platform_suffix"
                profile_entry["build_options"][key] = build_options_actual["active_platform"]
            if build_options_actual.get("flash_definition"):
                key = "flash_definition" if should_record_exact_path(build_options_actual["flash_definition"]) else "flash_definition_suffix"
                profile_entry["build_options"][key] = build_options_actual["flash_definition"]
        for relative_name, entry in report["pe_sections"].items():
            if entry.get("sections"):
                profile_entry["pe_sections"][relative_name] = [
                    {"name": section["name"], "size": section["size"]}
                    for section in entry["sections"]
                ]
        args.emit_profile.parent.mkdir(parents=True, exist_ok=True)
        args.emit_profile.write_text(
            json.dumps(generated_profile, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"Generated profile: {args.emit_profile}")

    print(f"Validation profile: {resolved_profile_name}")
    if resolved_profile_name != args.profile:
        print(f"Requested profile alias: {args.profile}")
    print(f"Build directory: {build_dir}")
    print(
        "Hash results: "
        f"{matched} matched, {mismatched} mismatched, {missing} missing"
    )
    if build_option_status == "match":
        print("BuildOptions structure: match")
    elif build_option_status == "mismatch":
        print("BuildOptions structure: mismatch")
        for mismatch in report["build_options"]["mismatches"]:
            print(f"  - {mismatch}")
    else:
        print("BuildOptions structure: skipped")

    if section_mismatches:
        print(f"PE/COFF section mismatches: {section_mismatches}")

    if args.report_json:
        args.report_json.parent.mkdir(parents=True, exist_ok=True)
        args.report_json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"Structural report: {args.report_json}")

    failed = mismatched or missing or build_option_status == "mismatch" or section_mismatches
    if failed:
        sys.stdout.flush()
        print(
            "WARNING: built artefacts differ from the stored validation profile.",
            file=sys.stderr,
        )
        print(
            "         Review the hash and structural report above before treating this build as replay-identical.",
            file=sys.stderr,
        )

    return 1 if args.strict and failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
