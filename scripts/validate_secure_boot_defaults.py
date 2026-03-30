#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
import sys

from generate_microsoft_secure_boot_defaults import (
    DEFAULT_MANIFEST_PATH,
    DEFAULT_OUTPUT_DIR,
    check_named_files,
    load_manifest,
)


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
RAW_SECTION_HEADER_SIZE = 4
RAW_SECTION_TYPE = 0x19
SOURCE_PATH_RE = re.compile(r"GenSec .* (?P<source>/.*)$")
GUID_BY_VARIABLE = {
    "PK.bin": "85254ea7-4759-4fc4-82d4-5eed5fb0a4a0",
    "KEK.bin": "6f64916e-9f7a-4c35-b952-cd041efb05a3",
    "DB.bin": "c491d352-7623-4843-accc-2791a7574421",
    "DBX.bin": "5740766a-718e-4dc0-9935-c36f7d3f884f",
}

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate that a custom firmware build embeds the Microsoft Secure Boot defaults."
    )
    parser.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument("--payload-dir", type=pathlib.Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--manifest", type=pathlib.Path, default=DEFAULT_MANIFEST_PATH)
    parser.add_argument("--board", default="O6")
    parser.add_argument("--target", default="RELEASE_GCC5")
    parser.add_argument("--arch", default="aarch64")
    return parser.parse_args()


def resolve_build_dir(args: argparse.Namespace) -> pathlib.Path:
    if args.build_dir:
        return args.build_dir.resolve()
    return args.repo_root.resolve() / "src" / "Build" / args.board / args.target


def read_raw_section_payload(path: pathlib.Path) -> bytes:
    data = path.read_bytes()
    if len(data) < RAW_SECTION_HEADER_SIZE:
        raise ValueError(f"{path} is too small to contain an EFI raw section")
    section_size = data[0] | (data[1] << 8) | (data[2] << 16)
    section_type = data[3]
    if section_type != RAW_SECTION_TYPE:
        raise ValueError(f"{path} has unexpected section type 0x{section_type:02x}")
    if section_size == 0xFFFFFF:
        section_size = len(data)
    if section_size > len(data):
        raise ValueError(f"{path} declares section size {section_size} beyond file size {len(data)}")
    return data[RAW_SECTION_HEADER_SIZE:section_size]


def read_source_path(path: pathlib.Path) -> pathlib.Path:
    match = SOURCE_PATH_RE.search(path.read_text(encoding="utf-8").strip())
    if not match:
        raise ValueError(f"Could not parse GenSec source path from {path}")
    return pathlib.Path(match.group("source")).resolve()


def relative_suffix(path: pathlib.Path, root: pathlib.Path) -> str | None:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return None


def source_paths_match(actual: pathlib.Path, expected: pathlib.Path, expected_suffix: str | None) -> bool:
    if actual == expected:
        return True
    return expected_suffix is not None and actual.as_posix().endswith(expected_suffix)


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    build_dir = resolve_build_dir(args)
    payload_dir = args.payload_dir.resolve()
    manifest = load_manifest(args.manifest.resolve())

    if args.arch != str(manifest["target_arch"]):
        print(
            f"manifest {args.manifest} is pinned for {manifest['target_arch']}, got --arch {args.arch}",
            file=sys.stderr,
        )
        return 1

    failures = check_named_files(
        payload_dir,
        manifest["outputs"],  # type: ignore[arg-type]
        label="payload",
    )
    for payload_name, guid in GUID_BY_VARIABLE.items():
        section_dir = build_dir / "FV" / "Ffs" / f"{guid}FVMAIN"
        raw_path = section_dir / f"{guid}SEC1.raw"
        trace_path = section_dir / f"{guid}SEC1.raw.txt"
        expected_source = (payload_dir / payload_name).resolve()
        expected_suffix = relative_suffix(expected_source, repo_root)

        if not raw_path.is_file():
            failures.append(f"missing raw section: {raw_path}")
            continue
        if not trace_path.is_file():
            failures.append(f"missing GenSec trace: {trace_path}")
            continue

        expected_payload_path = payload_dir / payload_name
        if not expected_payload_path.is_file():
            failures.append(f"missing payload: {expected_payload_path}")
            continue

        actual_payload = read_raw_section_payload(raw_path)
        expected_payload = expected_payload_path.read_bytes()
        if actual_payload != expected_payload:
            failures.append(f"payload mismatch for {payload_name}: {raw_path}")

        actual_source = read_source_path(trace_path)
        if not source_paths_match(actual_source, expected_source, expected_suffix):
            failures.append(
                f"unexpected GenSec source for {payload_name}: expected {expected_source}, got {actual_source}"
            )

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print(f"Validated Microsoft Secure Boot defaults in {build_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
