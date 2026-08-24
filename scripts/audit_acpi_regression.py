#!/usr/bin/env python3

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import tempfile
from typing import Any


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
DEFAULT_BASELINE_FILE = REPO_ROOT / "validation" / "acpi-audit-baselines.json"
DEFAULT_PROFILE = "upstream-1.2.1-bookworm"
IASL_RESOLVER = REPO_ROOT / "scripts" / "ensure_iasl.sh"

IASL_DIAGNOSTIC_RE = re.compile(
    r"^(?P<source>.+?)\((?P<line>\d+)\)\s*:\s*"
    r"(?P<kind>warning|remark|error)\s+(?P<code>\d+)\s+-\s+(?P<message>.+)$",
    re.IGNORECASE,
)
IASL_SUMMARY_RE = re.compile(
    r"Compilation\s+(?:successful|failed)\.\s+"
    r"(?P<errors>\d+)\s+Errors,\s+"
    r"(?P<warnings>\d+)\s+Warnings,\s+"
    r"(?P<remarks>\d+)\s+Remarks"
    r"(?:,\s+(?P<optimizations>\d+)\s+Optimizations,\s+"
    r"(?P<constants_folded>\d+)\s+Constants Folded)?",
    re.IGNORECASE,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit built ACPI tables and compiler diagnostics against a checked baseline."
    )
    parser.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument("--board", default="O6")
    parser.add_argument("--target", default="RELEASE_GCC")
    parser.add_argument("--profile", default=DEFAULT_PROFILE)
    parser.add_argument("--baseline-file", type=pathlib.Path, default=DEFAULT_BASELINE_FILE)
    parser.add_argument("--report-json", type=pathlib.Path)
    parser.add_argument("--emit-baseline", type=pathlib.Path)
    parser.add_argument("--emit-profile-name")
    parser.add_argument("--iasl", type=pathlib.Path)
    return parser.parse_args()


def resolve_build_dir(args: argparse.Namespace) -> pathlib.Path:
    if args.build_dir:
        return args.build_dir.resolve()
    return args.repo_root.resolve() / "src" / "Build" / args.board / args.target


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


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


def acpi_output_dirs(build_dir: pathlib.Path, board: str) -> list[pathlib.Path]:
    return [
        build_dir
        / "AARCH64"
        / "Platform"
        / "Radxa"
        / "Orion"
        / board
        / "Drivers"
        / "AcpiPlatfomTables"
        / "AcpiPlatfomTables"
        / "OUTPUT",
        build_dir
        / "AARCH64"
        / "Platform"
        / "CIX"
        / "Sky1"
        / "Drivers"
        / "AcpiSocTables"
        / "AcpiSocTables"
        / "OUTPUT",
    ]


def discover_acpi_tables(build_dir: pathlib.Path, board: str) -> dict[str, dict[str, Any]]:
    tables: dict[str, dict[str, Any]] = {}
    for output_dir in acpi_output_dirs(build_dir, board):
        if not output_dir.is_dir():
            continue
        for suffix in ("*.aml", "*.acpi"):
            for path in sorted(output_dir.glob(suffix)):
                relative = path.relative_to(build_dir).as_posix()
                tables[relative] = {
                    "path": relative,
                    "size": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
    return tables


def resolve_iasl(explicit_path: pathlib.Path | None = None) -> pathlib.Path:
    command = [str(IASL_RESOLVER)]
    if explicit_path is not None:
        command.extend(["--verify", str(explicit_path)])
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "unable to resolve the pinned iasl compiler")
    return pathlib.Path(result.stdout.strip())


def run_iasl(source_path: pathlib.Path, iasl_path: pathlib.Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="edk2-cix-acpi-audit-") as tempdir_text:
        prefix = pathlib.Path(tempdir_text) / source_path.stem
        result = subprocess.run(
            [str(iasl_path), "-vi", "-p", str(prefix), str(source_path)],
            check=False,
            capture_output=True,
            text=True,
        )

    diagnostics: list[dict[str, Any]] = []
    code_counts: dict[str, collections.Counter[str]] = {
        "warning": collections.Counter(),
        "remark": collections.Counter(),
        "error": collections.Counter(),
    }
    code_messages: dict[str, dict[str, str]] = {
        "warning": {},
        "remark": {},
        "error": {},
    }
    summary: dict[str, int] = {}

    for line in (result.stdout + result.stderr).splitlines():
        match = IASL_DIAGNOSTIC_RE.match(line.strip())
        if match:
            kind = match.group("kind").lower()
            code = match.group("code")
            message = match.group("message").strip()
            diagnostics.append(
                {
                    "line": int(match.group("line")),
                    "kind": kind,
                    "code": code,
                    "message": message,
                }
            )
            code_counts[kind][code] += 1
            code_messages[kind].setdefault(code, message)
            continue

        summary_match = IASL_SUMMARY_RE.search(line)
        if summary_match:
            summary = {
                "errors": int(summary_match.group("errors")),
                "warnings": int(summary_match.group("warnings")),
                "remarks": int(summary_match.group("remarks")),
                "optimizations": int(summary_match.group("optimizations") or 0),
                "constants_folded": int(summary_match.group("constants_folded") or 0),
            }

    return {
        "source": source_path.name,
        "status": "match" if result.returncode == 0 else "compiler-error",
        "summary": summary,
        "warning_codes": dict(sorted(code_counts["warning"].items())),
        "remark_codes": dict(sorted(code_counts["remark"].items())),
        "error_codes": dict(sorted(code_counts["error"].items())),
        "warning_messages": {
            code: code_messages["warning"][code] for code in sorted(code_messages["warning"])
        },
        "remark_messages": {
            code: code_messages["remark"][code] for code in sorted(code_messages["remark"])
        },
        "error_messages": {
            code: code_messages["error"][code] for code in sorted(code_messages["error"])
        },
        "diagnostics": diagnostics,
    }


def discover_iasl_sources(
    build_dir: pathlib.Path,
    board: str,
    iasl_path: pathlib.Path,
) -> dict[str, dict[str, Any]]:
    sources = {
        "platform_ssdt": build_dir
        / "AARCH64"
        / "Platform"
        / "Radxa"
        / "Orion"
        / board
        / "Drivers"
        / "AcpiPlatfomTables"
        / "AcpiPlatfomTables"
        / "OUTPUT"
        / "Ssdt.iiii",
        "soc_dsdt": build_dir
        / "AARCH64"
        / "Platform"
        / "CIX"
        / "Sky1"
        / "Drivers"
        / "AcpiSocTables"
        / "AcpiSocTables"
        / "OUTPUT"
        / "Dsdt.iiii",
    }

    audits: dict[str, dict[str, Any]] = {}
    for key, source_path in sources.items():
        if source_path.is_file():
            audits[key] = run_iasl(source_path, iasl_path)
            audits[key]["path"] = source_path.relative_to(build_dir).as_posix()
        else:
            audits[key] = {
                "path": source_path.relative_to(build_dir).as_posix(),
                "status": "missing",
            }
    return audits


def gather_acpi_audit(
    build_dir: pathlib.Path,
    board: str,
    target: str,
    iasl_path: pathlib.Path,
) -> dict[str, Any]:
    return {
        "board": board,
        "target": target,
        "tables": discover_acpi_tables(build_dir, board),
        "iasl": discover_iasl_sources(build_dir, board, iasl_path),
    }


def compare_audits(expected: dict[str, Any], actual: dict[str, Any]) -> list[str]:
    def normalize_table(entry: dict[str, Any]) -> dict[str, Any]:
        return dict(entry)

    def normalize_iasl(entry: dict[str, Any]) -> dict[str, Any]:
        return {
            "path": entry.get("path"),
            "status": entry.get("status"),
            "summary": entry.get("summary", {}),
            "warning_codes": entry.get("warning_codes", {}),
            "remark_codes": entry.get("remark_codes", {}),
            "error_codes": entry.get("error_codes", {}),
            "warning_messages": entry.get("warning_messages", {}),
            "remark_messages": entry.get("remark_messages", {}),
            "error_messages": entry.get("error_messages", {}),
        }

    mismatches: list[str] = []

    expected_tables = {
        name: normalize_table(entry)
        for name, entry in expected.get("tables", {}).items()
    }
    actual_tables = {
        name: normalize_table(entry)
        for name, entry in actual.get("tables", {}).items()
    }
    if expected_tables != actual_tables:
        missing = sorted(set(expected_tables) - set(actual_tables))
        unexpected = sorted(set(actual_tables) - set(expected_tables))
        changed = sorted(
            name for name in set(expected_tables) & set(actual_tables) if expected_tables[name] != actual_tables[name]
        )
        if missing:
            mismatches.append(f"Missing ACPI tables: {', '.join(missing)}")
        if unexpected:
            mismatches.append(f"Unexpected ACPI tables: {', '.join(unexpected)}")
        if changed:
            mismatches.append(f"Changed ACPI tables: {', '.join(changed)}")

    expected_iasl = {
        name: normalize_iasl(entry)
        for name, entry in expected.get("iasl", {}).items()
    }
    actual_iasl = {
        name: normalize_iasl(entry)
        for name, entry in actual.get("iasl", {}).items()
    }
    if expected_iasl != actual_iasl:
        missing = sorted(set(expected_iasl) - set(actual_iasl))
        unexpected = sorted(set(actual_iasl) - set(expected_iasl))
        changed = sorted(
            name for name in set(expected_iasl) & set(actual_iasl) if expected_iasl[name] != actual_iasl[name]
        )
        if missing:
            mismatches.append(f"Missing IASL audits: {', '.join(missing)}")
        if unexpected:
            mismatches.append(f"Unexpected IASL audits: {', '.join(unexpected)}")
        if changed:
            mismatches.append(f"Changed IASL audits: {', '.join(changed)}")

    return mismatches


def emit_baseline(
    path: pathlib.Path,
    profile_name: str,
    board: str,
    target: str,
    audit: dict[str, Any],
) -> None:
    payload = load_baseline_payload(path)
    profiles = payload.setdefault("profiles", {})
    profile = profiles.setdefault(
        profile_name,
        {
            "description": (
                "Generated from deterministic upstream replay outputs for ACPI "
                "regression checks."
            ),
            "boards": {},
        },
    )
    boards = profile.setdefault("boards", {})
    boards[board] = {
        "target": target,
        "acpi": audit,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    build_dir = resolve_build_dir(args)
    try:
        iasl_path = resolve_iasl(args.iasl)
    except RuntimeError as exc:
        print(f"Unable to run ACPI audit: {exc}", file=sys.stderr)
        return 2
    audit = gather_acpi_audit(build_dir, args.board, args.target, iasl_path)

    if args.emit_baseline:
        profile_name = args.emit_profile_name or args.profile
        emit_baseline(args.emit_baseline, profile_name, args.board, args.target, audit)
        print(f"Generated ACPI audit baseline: {args.emit_baseline}")
        return 0

    try:
        profiles = load_profiles(args.baseline_file.resolve())
        profile_meta, board_profile = resolve_profile(profiles, args.profile, args.board, args.target)
    except KeyError:
        print(
            f"Unknown ACPI audit profile '{args.profile}' in {args.baseline_file}",
            file=sys.stderr,
        )
        return 2
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    expected_audit = board_profile.get("acpi")
    if not isinstance(expected_audit, dict):
        print(
            f"Profile '{args.profile}' board '{args.board}' is missing an ACPI audit baseline",
            file=sys.stderr,
        )
        return 2

    mismatches = compare_audits(expected_audit, audit)
    report = {
        "profile": args.profile,
        "description": profile_meta.get("description"),
        "board": args.board,
        "target": args.target,
        "build_dir": str(build_dir),
        "status": "match" if not mismatches else "mismatch",
        "mismatches": mismatches,
        "acpi": audit,
    }

    print(f"ACPI audit profile: {args.profile}")
    print(f"Build directory: {build_dir}")
    if mismatches:
        print("ACPI regression audit: mismatch")
        for mismatch in mismatches:
            print(f"  - {mismatch}")
    else:
        print("ACPI regression audit: match")

    if args.report_json:
        args.report_json.parent.mkdir(parents=True, exist_ok=True)
        args.report_json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"ACPI audit report: {args.report_json}")

    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
