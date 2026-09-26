#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

CACHE_LAYOUT_VERSION = 1


def _hash_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def file_fingerprint(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def optional_file_fingerprint(path: Path | None) -> str:
    if path is None or not path.is_file():
        return "absent"
    return file_fingerprint(path)


def compiler_fingerprint(compiler: str) -> str:
    resolved = shutil.which(compiler) or compiler
    version = "unavailable"
    machine = "unknown"

    try:
        version_output = subprocess.run(
            [resolved, "--version"],
            check=True,
            capture_output=True,
            text=True,
        )
        first_line = version_output.stdout.splitlines()[0].strip()
        if first_line:
            version = first_line
    except (FileNotFoundError, subprocess.CalledProcessError, IndexError):
        pass

    try:
        machine_output = subprocess.run(
            [resolved, "-dumpmachine"],
            check=True,
            capture_output=True,
            text=True,
        )
        dumped = machine_output.stdout.strip()
        if dumped:
            machine = dumped
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass

    return f"{resolved}|{machine}|{version}"


def _should_skip_tree_entry(
    root: Path,
    path: Path,
    *,
    excluded_prefixes: tuple[str, ...],
    excluded_exact: tuple[str, ...],
    excluded_suffixes: tuple[str, ...],
) -> bool:
    rel = path.relative_to(root).as_posix()
    if rel in excluded_exact:
        return True
    if any(rel == prefix or rel.startswith(f"{prefix}/") for prefix in excluded_prefixes):
        return True
    if any(rel.endswith(suffix) for suffix in excluded_suffixes):
        return True
    return False


def tree_fingerprint(
    root: Path,
    *,
    excluded_prefixes: tuple[str, ...] = (),
    excluded_exact: tuple[str, ...] = (),
    excluded_suffixes: tuple[str, ...] = (),
) -> str:
    digest = hashlib.sha256()
    digest.update(f"tree-v{CACHE_LAYOUT_VERSION}\0".encode("utf-8"))
    digest.update(str(root.resolve()).encode("utf-8"))
    digest.update(b"\0")

    for path in sorted(root.rglob("*")):
        if _should_skip_tree_entry(
            root,
            path,
            excluded_prefixes=excluded_prefixes,
            excluded_exact=excluded_exact,
            excluded_suffixes=excluded_suffixes,
        ):
            continue
        rel = path.relative_to(root).as_posix()
        stat = path.lstat()
        mode_bits = stat.st_mode & 0o777
        digest.update(rel.encode("utf-8"))
        digest.update(b"\0")
        digest.update(f"{mode_bits:o}".encode("ascii"))
        digest.update(b"\0")
        if path.is_symlink():
            digest.update(b"link\0")
            digest.update(os.readlink(path).encode("utf-8"))
            digest.update(b"\0")
            continue
        if not path.is_file():
            continue
        digest.update(b"file\0")
        with path.open("rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
        digest.update(b"\0")
    return digest.hexdigest()


def _key_payload_hash(payload: dict[str, str]) -> str:
    normalized = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return _hash_bytes(normalized)


@dataclass(frozen=True)
class CachePlan:
    cert_create_key: str
    bl31_key: str
    tee_key: str


def build_cache_plan(
    *,
    cert_create_tree_fingerprint: str,
    tfa_tree_fingerprint: str,
    tee_tree_fingerprint: str,
    helper_fingerprint: str,
    host_compiler_fingerprint: str,
    cross_compiler_fingerprint: str,
    mode: str,
    stmm_fingerprint: str,
) -> CachePlan:
    cert_create_key = _key_payload_hash(
        {
            "version": str(CACHE_LAYOUT_VERSION),
            "kind": "cert_create",
            "helper": helper_fingerprint,
            "host_compiler": host_compiler_fingerprint,
            "cert_create_tree": cert_create_tree_fingerprint,
        }
    )
    bl31_key = _key_payload_hash(
        {
            "version": str(CACHE_LAYOUT_VERSION),
            "kind": "bl31",
            "helper": helper_fingerprint,
            "mode": mode,
            "cross_compiler": cross_compiler_fingerprint,
            "tfa_tree": tfa_tree_fingerprint,
        }
    )
    tee_key = _key_payload_hash(
        {
            "version": str(CACHE_LAYOUT_VERSION),
            "kind": "tee",
            "helper": helper_fingerprint,
            "cross_compiler": cross_compiler_fingerprint,
            "tee_tree": tee_tree_fingerprint,
            "stmm": stmm_fingerprint,
        }
    )
    return CachePlan(
        cert_create_key=cert_create_key,
        bl31_key=bl31_key,
        tee_key=tee_key,
    )


def compute_cache_plan(
    *,
    tfa_dir: Path,
    tee_dir: Path,
    helper_script: Path,
    mode: str,
    cross_compiler: str,
    host_compiler: str,
    stmm_path: Path | None,
) -> CachePlan:
    cert_create_dir = tfa_dir / "tools" / "cert_create"
    cert_create_tree_fingerprint = tree_fingerprint(
        cert_create_dir,
        excluded_prefixes=("build", ".git", "__pycache__"),
        excluded_exact=("cert_create",),
        excluded_suffixes=(".o", ".d", ".cmd"),
    )
    tfa_tree_fingerprint = tree_fingerprint(
        tfa_dir,
        excluded_prefixes=("build", ".git", "__pycache__"),
        excluded_exact=("tools/cert_create/cert_create",),
        excluded_suffixes=(".pyc", ".o", ".d", ".cmd"),
    )
    tee_tree_fingerprint = tree_fingerprint(
        tee_dir,
        excluded_prefixes=("out", ".git", "__pycache__"),
        excluded_exact=("tee.bin",),
        excluded_suffixes=(".pyc",),
    )
    return build_cache_plan(
        cert_create_tree_fingerprint=cert_create_tree_fingerprint,
        tfa_tree_fingerprint=tfa_tree_fingerprint,
        tee_tree_fingerprint=tee_tree_fingerprint,
        helper_fingerprint=file_fingerprint(helper_script),
        host_compiler_fingerprint=compiler_fingerprint(host_compiler),
        cross_compiler_fingerprint=compiler_fingerprint(cross_compiler),
        mode=mode,
        stmm_fingerprint=optional_file_fingerprint(stmm_path),
    )


def emit_shell_assignments(cache_root: Path, plan: CachePlan) -> str:
    values = {
        "CIX_RELEASE_CACHE_ROOT": cache_root,
        "CERT_CREATE_CACHE_DIR": cache_root / "cert_create" / plan.cert_create_key,
        "CERT_CREATE_CACHE_BIN": cache_root / "cert_create" / plan.cert_create_key / "cert_create",
        "BL31_CACHE_DIR": cache_root / "bl31" / plan.bl31_key,
        "BL31_CACHE_BIN": cache_root / "bl31" / plan.bl31_key / "bl31.bin",
        "TEE_CACHE_DIR": cache_root / "tee" / plan.tee_key,
        "TEE_CACHE_BIN": cache_root / "tee" / plan.tee_key / "tee-raw.bin",
        "CERT_CREATE_CACHE_KEY": plan.cert_create_key,
        "BL31_CACHE_KEY": plan.bl31_key,
        "TEE_CACHE_KEY": plan.tee_key,
    }
    return "\n".join(f"{name}={shlex.quote(str(value))}" for name, value in values.items())


def main() -> int:
    parser = argparse.ArgumentParser(description="Compute cache locations for curated CIX bootloader2 intermediates.")
    parser.add_argument("--cache-root", required=True)
    parser.add_argument("--tfa-dir", required=True)
    parser.add_argument("--tee-dir", required=True)
    parser.add_argument("--helper-script", required=True)
    parser.add_argument("--mode", choices=("release", "debug"), required=True)
    parser.add_argument("--cross-compiler", required=True)
    parser.add_argument("--host-compiler", default="cc")
    parser.add_argument("--stmm-path")
    parser.add_argument("--shell", action="store_true")
    args = parser.parse_args()

    plan = compute_cache_plan(
        tfa_dir=Path(args.tfa_dir),
        tee_dir=Path(args.tee_dir),
        helper_script=Path(args.helper_script),
        mode=args.mode,
        cross_compiler=args.cross_compiler,
        host_compiler=args.host_compiler,
        stmm_path=Path(args.stmm_path) if args.stmm_path else None,
    )

    cache_root = Path(args.cache_root)
    if args.shell:
        print(emit_shell_assignments(cache_root, plan))
        return 0

    print(
        json.dumps(
            {
                "cache_root": str(cache_root),
                "cert_create_key": plan.cert_create_key,
                "bl31_key": plan.bl31_key,
                "tee_key": plan.tee_key,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
