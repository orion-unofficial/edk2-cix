#!/usr/bin/env python3
"""Resolve the safe user-facing firmware build profiles."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex

from reconstruction_common import ReconstructionError, default_release, load_json, main_wrapper, repo_root


TRUE_TOKENS = {"1", "true", "on", "yes"}
FALSE_TOKENS = {"0", "false", "off", "no"}
BOOLEAN_CUSTOM_OPTIONS = {
    "DEBUG_ON_UART3",
    "DEBUG_VERBOSE",
    "ENABLE_EXPERIMENTAL_UEFI_SETTINGS",
    "UART3_ENABLE",
}
SHELL_FIELDS = {
    "profile": "PROFILE_EFFECTIVE",
    "build_kind": "PROFILE_BUILD_KIND",
    "release": "PROFILE_RELEASE",
    "artefact_mode": "PROFILE_ARTEFACT_MODE",
    "enable_firmware_fixes": "PROFILE_ENABLE_FIRMWARE_FIXES",
    "cix_early_boot_release": "PROFILE_CIX_EARLY_BOOT_RELEASE",
    "replay_version": "PROFILE_REPLAY_VERSION",
}


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo-root", type=Path)
    p.add_argument("--profile", default=os.environ.get("PROFILE", ""))
    p.add_argument("--release", default=os.environ.get("RELEASE", ""))
    p.add_argument("--artefact-mode", default=os.environ.get("ARTEFACT_MODE_OVERRIDE", ""))
    p.add_argument("--firmware-target", default=os.environ.get("FIRMWARE_TARGET", "RELEASE"))
    p.add_argument("--enable-firmware-fixes", default=os.environ.get("ENABLE_FIRMWARE_FIXES", ""))
    p.add_argument("--cix-release", default=os.environ.get("CIX_RELEASE", ""))
    p.add_argument("--custom-option", action="append", default=[])
    p.add_argument("--field", choices=tuple(SHELL_FIELDS))
    return p


def resolve_profile(
    repo: Path,
    *,
    requested_profile: str = "",
    release_override: str = "",
    artefact_mode_override: str = "",
    firmware_target: str = "RELEASE",
    firmware_fixes_override: str = "",
    cix_release_override: str = "",
    custom_options: list[str] | None = None,
) -> dict[str, str]:
    policy = load_json(repo, "config/policies.json").get("firmware_profile_policy", {})
    profiles = policy.get("profiles", {})
    selected = requested_profile.strip() or str(policy.get("default_profile", "")).strip()
    if selected not in profiles:
        available = ", ".join(sorted(profiles)) or "none configured"
        raise ReconstructionError(f"PROFILE must be one of: {available}; got: {selected or '<empty>'}")

    configured = profiles[selected]
    result = {
        "profile": selected,
        "build_kind": str(configured.get("build_kind", "")),
        "release": str(configured.get("release", "")),
        "artefact_mode": str(configured.get("artefact_mode", "")),
        "enable_firmware_fixes": "true" if configured.get("enable_firmware_fixes") else "false",
        "cix_early_boot_release": str(configured.get("cix_early_boot_release") or ""),
        "replay_version": str(configured.get("replay_version", "")),
    }
    if result["release"] == "unofficial-policy-default":
        result["release"] = default_release(repo)

    problems: list[str] = []
    if release_override.strip() and release_override.strip() != result["release"]:
        problems.append(
            f"PROFILE={selected} fixes RELEASE={result['release']}; use a low-level build target for an explicit source target"
        )
    if artefact_mode_override.strip() and artefact_mode_override.strip() != result["artefact_mode"]:
        problems.append(
            f"PROFILE={selected} requires ARTEFACT_MODE={result['artefact_mode']}; got: {artefact_mode_override.strip()}"
        )
    if firmware_target.strip().upper() != "RELEASE" and result["build_kind"] == "deterministic-replay":
        problems.append("the upstream byte-identical profile requires FIRMWARE_TARGET=RELEASE")

    fixes = firmware_fixes_override.strip().lower()
    if fixes and fixes not in TRUE_TOKENS | FALSE_TOKENS:
        problems.append(
            "ENABLE_FIRMWARE_FIXES must be one of: true, false, 1, 0, on, off, yes, no; "
            f"got: {firmware_fixes_override.strip()}"
        )
    elif fixes in TRUE_TOKENS:
        if result["artefact_mode"] != "custom":
            problems.append("ENABLE_FIRMWARE_FIXES=true is incompatible with PROFILE=upstream")
        else:
            result["enable_firmware_fixes"] = "true"
    elif fixes in FALSE_TOKENS:
        result["enable_firmware_fixes"] = "false"

    cix_release = cix_release_override.strip().lower().lstrip("v")
    if cix_release and cix_release != result["cix_early_boot_release"]:
        expected = result["cix_early_boot_release"] or "no CIX early-boot replacement"
        problems.append(f"PROFILE={selected} requires {expected}; got CIX_RELEASE={cix_release_override.strip()}")

    if result["artefact_mode"] != "custom":
        active_options = []
        for item in custom_options or []:
            name, separator, value = item.partition("=")
            if not separator or not name.strip():
                problems.append(f"invalid --custom-option value: {item}")
                continue
            cleaned_value = value.strip()
            if cleaned_value and (
                name.strip() not in BOOLEAN_CUSTOM_OPTIONS or cleaned_value.lower() not in FALSE_TOKENS
            ):
                active_options.append(name.strip())
        if active_options:
            problems.append(
                "PROFILE=upstream does not permit custom firmware option(s): " + ", ".join(active_options)
            )

    for required in ("build_kind", "release", "artefact_mode"):
        if not result[required]:
            problems.append(f"PROFILE={selected} has no configured {required.replace('_', ' ')}")
    if result["build_kind"] == "deterministic-replay" and not result["replay_version"]:
        problems.append(f"PROFILE={selected} has no configured replay version")
    if problems:
        raise ReconstructionError("firmware profile resolution failed:\n" + "\n".join(f"  - {item}" for item in problems))
    return result


def main() -> None:
    args = parser().parse_args()
    repo = args.repo_root.resolve() if args.repo_root else repo_root(Path(__file__).resolve())
    result = resolve_profile(
        repo,
        requested_profile=args.profile,
        release_override=args.release,
        artefact_mode_override=args.artefact_mode,
        firmware_target=args.firmware_target,
        firmware_fixes_override=args.enable_firmware_fixes,
        cix_release_override=args.cix_release,
        custom_options=args.custom_option,
    )
    if args.field:
        print(result[args.field])
        return
    for field, shell_name in SHELL_FIELDS.items():
        print(f"{shell_name}={shlex.quote(result[field])}")


if __name__ == "__main__":
    main_wrapper(main)
