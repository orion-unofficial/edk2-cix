#!/usr/bin/env python3
"""Validate user-facing firmware build variables before rendering a release."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path

from reconstruction_common import ReconstructionError, main_wrapper, release_entry, repo_root, show_file
from debug_build_policy import preflight
from validate_release_inputs import source_fdf, validate_inputs
from validate_firmware_chain import CIX_KEY_SHA256, CIX_SIGNING_KEYS


VALID_ARTEFACT_MODES = {"custom", "upstream"}
VALID_FIRMWARE_TARGETS = {"RELEASE", "DEBUG"}
VALID_BOARDS = {"O6", "O6N"}
VALID_FIRMWARE_DISTROS = {"bookworm", "trixie"}
VALID_BUILDBOX_PLATFORMS = {"linux/amd64", "linux/arm64"}
VALID_CORE_ORDERS = {"cix", "conventional", "performance"}
CIX_SIGNING_HELPER_SHA256 = "1c5d06f76422759f2bcd1067c504337b5fd9791c190417a6652629af9933c2c7"
TRUE_TOKENS = {"1", "true", "on", "yes"}
FALSE_TOKENS = {"0", "false", "off", "no"}
SIGNING_CERT_NAMES = ("trusted_key_no.crt", "nt_fw_cert.crt", "nt_fw_key.crt")


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--target", default=os.environ.get("MAKE_TARGET", ""))
    p.add_argument("--repo-root", type=Path)
    return p


def env(name: str) -> str:
    return os.environ.get(name, "").strip()


def require_choice(name: str, value: str, allowed: set[str], problems: list[str]) -> None:
    if value and value not in allowed:
        allowed_text = ", ".join(sorted(allowed))
        problems.append(f"{name} must be one of: {allowed_text}; got: {value}")


def require_boolean(name: str, value: str, problems: list[str]) -> None:
    if not value:
        return
    lowered = value.lower()
    if lowered not in TRUE_TOKENS | FALSE_TOKENS:
        problems.append(f"{name} must be one of: true, false, 1, 0, on, off, yes, no; got: {value}")


def require_uint32(name: str, value: str, problems: list[str]) -> None:
    if not value:
        return
    try:
        parsed = int(value, 0)
    except ValueError:
        problems.append(f"{name} must be a decimal or 0x-prefixed 32-bit integer; got: {value}")
        return
    if not 0 <= parsed <= 0xFFFFFFFF:
        problems.append(f"{name} must fit in an unsigned 32-bit value; got: {value}")


def validate_release(repo: Path, problems: list[str]) -> None:
    selected = env("RELEASE")
    try:
        _branch, entry = release_entry(repo, selected or None)
        if (env("ARTEFACT_MODE") or "custom") == "custom":
            validate_inputs(repo, entry)
            if env("CIX_RELEASE"):
                validate_cix_source(repo, entry, problems)
            preflight(lambda path: show_file(repo, entry["source_ref"], path).decode(),
                      board=env("FIRMWARE_BOARD") or "O6", target=env("FIRMWARE_TARGET") or "RELEASE",
                      verbose=env("DEBUG_VERBOSE"), mask=env("DEBUG_PRINT_ERROR_LEVEL"),
                      force=env("FORCE_DEBUG_BUILD"),
                      allow_large=env("DEBUG_ALLOW_LARGE_IMAGE"),
                      fdf_override=source_fdf(repo, entry["source_ref"], env("FIRMWARE_BOARD") or "O6",
                                              env("ENABLE_EXPERIMENTAL_UEFI_SETTINGS").lower() in TRUE_TOKENS))
    except (ReconstructionError, ValueError) as exc:
        problems.append(str(exc))


def validate_cix_source(repo: Path, entry: dict, problems: list[str]) -> None:
    """Advertise CIX_RELEASE only for checkpoints with the reviewed helper and keys."""
    ref = entry["source_ref"]
    try:
        helper = show_file(repo, ref, "src/scripts/build_cix_release_bootloader2.sh")
        makefile = show_file(repo, ref, "src/Makefile")
        if hashlib.sha256(helper).hexdigest() != CIX_SIGNING_HELPER_SHA256 or b"CIX_RELEASE_SIGNING_KEYS_DIR" not in makefile:
            raise ReconstructionError("source-built CIX signing helper is absent or differs from the reviewed version")
        for name, expected in CIX_KEY_SHA256.items():
            data = show_file(repo, ref, f"{CIX_SIGNING_KEYS}/{name}")
            if hashlib.sha256(data).hexdigest() != expected:
                raise ReconstructionError(f"pinned CIX signing key differs: {name}")
    except ReconstructionError as exc:
        problems.append(f"CIX_RELEASE=1.2 is unavailable for {entry['source_ref']}: {exc}")


def validate_signing_cert_source(repo: Path, problems: list[str]) -> None:
    source_text = env("SIGNING_CERT_SOURCE_DIR")
    if not source_text:
        return
    source = Path(source_text).expanduser()
    if not source.is_absolute():
        source = repo / source
    if not source.is_dir():
        problems.append(f"SIGNING_CERT_SOURCE_DIR does not exist or is not a directory: {source}")
        return
    missing = [name for name in SIGNING_CERT_NAMES if not (source / name).is_file()]
    if missing:
        problems.append("SIGNING_CERT_SOURCE_DIR is missing required certificate file(s): " + ", ".join(missing))


def validate_feature_relationships(repo: Path, problems: list[str]) -> None:
    artefact_mode = env("ARTEFACT_MODE") or "custom"
    firmware_fixes = env("ENABLE_FIRMWARE_FIXES").lower()
    core_order = env("ENABLE_CORE_ORDER").lower()

    custom_only_booleans = (
        "DEBUG_ON_UART3",
        "UART3_ENABLE",
        "DEBUG_VERBOSE",
        "ENABLE_TF_A_FIXES",
        "ENABLE_FIRMWARE_FIXES",
        "ENABLE_EXPERIMENTAL_UEFI_SETTINGS",
    )
    if artefact_mode != "custom":
        if env("DEBUG_ALLOW_LARGE_IMAGE") == "1":
            problems.append("DEBUG_ALLOW_LARGE_IMAGE is only supported with ARTEFACT_MODE=custom")
        for name in custom_only_booleans:
            if env(name).lower() in TRUE_TOKENS:
                problems.append(f"{name} is only supported with ARTEFACT_MODE=custom")
        for name in ("DEBUG_PRINT_ERROR_LEVEL", "ENABLE_CORE_ORDER", "CIX_RELEASE"):
            if env(name):
                problems.append(f"{name} is only supported with ARTEFACT_MODE=custom")

    if core_order and core_order != "cix" and firmware_fixes not in TRUE_TOKENS:
        problems.append(f"ENABLE_CORE_ORDER={core_order} requires ENABLE_FIRMWARE_FIXES=true")
    if env("ENABLE_TF_A_FIXES").lower() in TRUE_TOKENS and env("CIX_RELEASE").lower().removeprefix("v") != "1.2":
        problems.append("ENABLE_TF_A_FIXES=true requires CIX_RELEASE=1.2")
    if env("CIX_RELEASE") and (env("FIRMWARE_BOARD") or "O6") != "O6":
        problems.append("CIX_RELEASE=1.2 currently supports only FIRMWARE_BOARD=O6")


def validate() -> None:
    args = parser().parse_args()
    repo = args.repo_root.resolve() if args.repo_root else repo_root(Path(__file__).resolve())
    problems: list[str] = []
    cix_release_raw = env("CIX_RELEASE").strip().lower()
    if cix_release_raw and cix_release_raw.removeprefix("v") != "1.2":
        problems.append("CIX_RELEASE supports only 1.2 (or v1.2)")

    require_choice("V", env("V") or "0", {"0", "1"}, problems)
    require_boolean("DEBUG", env("DEBUG") or "0", problems)
    require_choice("FORCE_DEBUG_BUILD", env("FORCE_DEBUG_BUILD"), {"0", "1"}, problems)
    require_choice("DEBUG_ALLOW_LARGE_IMAGE", env("DEBUG_ALLOW_LARGE_IMAGE"), {"0", "1"}, problems)
    require_choice("ARTEFACT_MODE", env("ARTEFACT_MODE") or "custom", VALID_ARTEFACT_MODES, problems)
    require_choice("FIRMWARE_TARGET", env("FIRMWARE_TARGET") or "RELEASE", VALID_FIRMWARE_TARGETS, problems)
    require_choice("FIRMWARE_BOARD", env("FIRMWARE_BOARD") or "O6", VALID_BOARDS, problems)
    require_choice("FIRMWARE_DISTRO", env("FIRMWARE_DISTRO"), VALID_FIRMWARE_DISTROS, problems)
    require_choice("BUILDBOX_PLATFORM", env("BUILDBOX_PLATFORM"), VALID_BUILDBOX_PLATFORMS, problems)
    require_choice("ENABLE_CORE_ORDER", env("ENABLE_CORE_ORDER").lower(), VALID_CORE_ORDERS, problems)

    for name in (
        "FIRMWARE_VALIDATE_ON_BUILD",
        "ENABLE_FIRMWARE_FIXES",
        "ENABLE_EXPERIMENTAL_UEFI_SETTINGS",
        "DEBUG_ON_UART3",
        "UART3_ENABLE",
        "DEBUG_VERBOSE",
        "ENABLE_TF_A_FIXES",
        "FORCE",
    ):
        require_boolean(name, env(name), problems)

    require_uint32("DEBUG_PRINT_ERROR_LEVEL", env("DEBUG_PRINT_ERROR_LEVEL"), problems)

    # Source-target resolution can be noticeably slower than checking simple
    # environment values, so report obvious user input errors before doing it.
    if not problems:
        validate_release(repo, problems)
        validate_signing_cert_source(repo, problems)
        validate_feature_relationships(repo, problems)

    if problems:
        detail = "\n".join(f"  - {problem}" for problem in problems)
        raise ReconstructionError(
            f"build variable validation failed before starting {args.target or 'the build'}:\n{detail}"
        )


if __name__ == "__main__":
    main_wrapper(validate)
