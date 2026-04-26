#!/usr/bin/env python3

from __future__ import annotations

import argparse
import datetime as dt
import json
import pathlib
import re
import shutil
import subprocess
import sys


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
DEFAULT_PROFILE_FILE = REPO_ROOT / "validation" / "expected-hashes.json"

VALID_ARTEFACT_MODES = {"custom", "upstream"}
VALID_FIRMWARE_TARGETS = {"RELEASE", "DEBUG"}
VALID_RAW_FIRMWARE_TARGETS = {f"{target}_GCC5" for target in VALID_FIRMWARE_TARGETS}
VALID_BOARDS = {"O6", "O6N"}
VALID_FIRMWARE_DISTROS = {"bookworm", "trixie"}
VALID_BUILDBOX_PLATFORMS = {"linux/amd64", "linux/arm64"}
VALID_CORE_ORDERS = {"cix", "conventional", "performance"}
VALID_CIX_RELEASES = {"1.2"}
TRUE_TOKENS = {"1", "true", "on", "yes"}
FALSE_TOKENS = {"0", "false", "off", "no"}
DEBUG_LIB_HEADER = REPO_ROOT / "src" / "edk2" / "MdePkg" / "Include" / "Library" / "DebugLib.h"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Normalize and validate user-facing make variables for edk2-cix."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    normalize_bool = subparsers.add_parser(
        "normalize-bool", help="Normalize a human-readable boolean to TRUE/FALSE"
    )
    normalize_bool.add_argument("--value", required=True)

    normalize_cix_release = subparsers.add_parser(
        "normalize-cix-release",
        help="Normalize a curated CIX release selector to its canonical form",
    )
    normalize_cix_release.add_argument("--value", required=True)

    normalize_u32 = subparsers.add_parser(
        "normalize-u32", help="Normalize a decimal or 0x-prefixed uint32 to canonical hex"
    )
    normalize_u32.add_argument("--value", required=True)

    describe_debug_level = subparsers.add_parser(
        "describe-debug-print-error-level",
        help="Describe a debug print error level mask in human-readable terms",
    )
    describe_debug_level.add_argument("--value", required=True)

    subparsers.add_parser(
        "max-debug-print-error-level",
        help="Print the mask that enables every known DEBUG_* print category",
    )

    subparsers.add_parser(
        "list-debug-print-error-level-bits",
        help="List the known DEBUG_* print bits from DebugLib.h",
    )

    validate = subparsers.add_parser(
        "validate", help="Validate a set of make variables"
    )
    validate.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    validate.add_argument("--profile-file", type=pathlib.Path, default=DEFAULT_PROFILE_FILE)
    validate.add_argument("--v")
    validate.add_argument("--artefact-mode")
    validate.add_argument("--firmware-target")
    validate.add_argument("--firmware-board")
    validate.add_argument("--firmware-distro")
    validate.add_argument("--firmware-validate-on-build")
    validate.add_argument("--validation-profile")
    validate.add_argument("--validation-board")
    validate.add_argument("--validation-target")
    validate.add_argument("--replay-input")
    validate.add_argument("--replay-build-options")
    validate.add_argument("--replay-build-date")
    validate.add_argument("--buildbox-platform")
    validate.add_argument("--buildbox-image")
    validate.add_argument("--debug-on-uart3")
    validate.add_argument("--uart3-enable")
    validate.add_argument("--debug-verbose")
    validate.add_argument("--debug-print-error-level")
    validate.add_argument("--enable-firmware-fixes")
    validate.add_argument("--enable-core-order")
    validate.add_argument("--cix-release")
    validate.add_argument("--enable-experimental-uefi-settings")
    validate.add_argument("--check-buildbox-image", action="store_true")
    return parser


def normalize_bool(value: str) -> str:
    lowered = value.strip().lower()
    if lowered in TRUE_TOKENS:
        return "TRUE"
    if lowered in FALSE_TOKENS:
        return "FALSE"
    raise ValueError(
        f"Expected one of true/false/1/0/on/off, got: {value}"
    )


def normalize_cix_release(value: str) -> str:
    lowered = value.strip().lower()
    if lowered.startswith("v"):
        lowered = lowered[1:]
    require_choice("CIX_RELEASE", lowered, VALID_CIX_RELEASES)
    return lowered


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 2


def require_choice(name: str, value: str, allowed: set[str]) -> None:
    if value not in allowed:
        allowed_text = ", ".join(sorted(allowed))
        raise ValueError(f"{name} must be one of: {allowed_text}; got: {value}")


def parse_u32(name: str, value: str) -> int:
    try:
        parsed = int(value, 0)
    except ValueError as exc:
        raise ValueError(
            f"{name} must be a decimal or 0x-prefixed 32-bit integer; got: {value}"
        ) from exc
    if not 0 <= parsed <= 0xFFFFFFFF:
        raise ValueError(
            f"{name} must fit in an unsigned 32-bit value; got: {value}"
        )
    return parsed


def format_u32(value: int) -> str:
    return f"0x{value:08X}"


def parse_debug_print_error_level_bits() -> list[tuple[str, int, str]]:
    in_section = False
    bits: list[tuple[str, int, str]] = []

    for raw_line in DEBUG_LIB_HEADER.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip()
        if "Declare bits for PcdDebugPrintErrorLevel" in line:
            in_section = True
            continue
        if not in_section:
            continue
        if "Aliases of debug message mask bits" in line:
            break

        match = re.match(
            r"^#define\s+(DEBUG_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+)\s*(?://\s*(.*))?$",
            line,
        )
        if match:
            name = match.group(1)
            value = int(match.group(2), 16)
            comment = (match.group(3) or "").strip()
            bits.append((name, value, comment))
            continue

        continuation = re.match(r"^\s*//\s*(.+)$", line)
        if continuation and bits:
            name, value, comment = bits[-1]
            extra = continuation.group(1).strip()
            bits[-1] = (name, value, f"{comment} {extra}".strip())

    if not bits:
        raise ValueError(f"Could not derive DEBUG_* print bits from {DEBUG_LIB_HEADER}")

    return bits


def debug_print_error_level_mask() -> int:
    mask = 0
    for _, value, _ in parse_debug_print_error_level_bits():
        mask |= value
    return mask


def validate_debug_print_error_level(name: str, value: str) -> int:
    parsed = parse_u32(name, value)
    unknown_bits = parsed & ~debug_print_error_level_mask()
    if unknown_bits:
        raise ValueError(
            f"{name} contains unknown or unsupported bits: {format_u32(unknown_bits)}; "
            "run 'make help-debug' for the accepted DEBUG_* values"
        )
    return parsed


def describe_debug_print_error_level_value(value: int) -> str:
    enabled = [
        name for name, bit_value, _ in parse_debug_print_error_level_bits()
        if value & bit_value
    ]
    if not enabled:
        return f"{format_u32(value)} -> none"
    return f"{format_u32(value)} -> {' | '.join(enabled)}"


def load_profiles(profile_file: pathlib.Path) -> dict[str, object]:
    data = json.loads(profile_file.read_text(encoding="utf-8"))
    profiles = data.get("profiles")
    if not isinstance(profiles, dict):
        raise ValueError(f"{profile_file} does not contain a top-level 'profiles' mapping")
    return profiles


def validate_profile(
    profile_file: pathlib.Path,
    profile_name: str,
    board: str | None,
    target: str | None,
) -> None:
    profiles = load_profiles(profile_file)
    profile = profiles.get(profile_name)
    if not isinstance(profile, dict):
        raise ValueError(f"Unknown validation profile '{profile_name}' in {profile_file}")

    if board is None:
        return

    board_profiles = profile.get("boards")
    if not isinstance(board_profiles, dict):
        legacy_board = profile.get("board")
        if legacy_board and legacy_board != board:
            raise ValueError(
                f"Validation profile '{profile_name}' is recorded for board "
                f"'{legacy_board}', not '{board}'"
            )
        legacy_target = profile.get("target")
        if target and legacy_target and legacy_target != target:
            raise ValueError(
                f"Validation profile '{profile_name}' is recorded for target "
                f"'{legacy_target}', not '{target}'"
            )
        return

    board_profile = board_profiles.get(board)
    if not isinstance(board_profile, dict):
        available = ", ".join(sorted(board_profiles))
        raise ValueError(
            f"Validation profile '{profile_name}' does not define board '{board}' "
            f"(available: {available})"
        )

    recorded_target = board_profile.get("target") or profile.get("target")
    if target and recorded_target and recorded_target != target:
        raise ValueError(
            f"Validation profile '{profile_name}' for board '{board}' is recorded "
            f"for target '{recorded_target}', not '{target}'"
        )


def validate_replay_input(
    input_path_text: str,
    board: str | None,
    replay_build_options: str | None,
    replay_build_date: str | None,
) -> None:
    input_path = pathlib.Path(input_path_text).expanduser().resolve()
    if not input_path.exists():
        raise ValueError(f"REPLAY_INPUT does not exist: {input_path}")

    if input_path.is_dir():
        flash_path = input_path / "cix_flash_all.bin"
        if not flash_path.is_file():
            raise ValueError(
                f"REPLAY_INPUT directory must contain cix_flash_all.bin: {input_path}"
            )
        if not (input_path / "BuildOptions").is_file() and not replay_build_date:
            raise ValueError(
                "REPLAY_INPUT directory is missing BuildOptions; supply "
                "REPLAY_BUILD_DATE=<iso8601> for a full replay build"
            )
        return

    suffixes = input_path.suffixes
    if suffixes[-1:] == [".deb"]:
        return
    if suffixes[-1:] == [".bin"]:
        if replay_build_options:
            replay_build_options_path = pathlib.Path(replay_build_options).expanduser().resolve()
            if not replay_build_options_path.is_file():
                raise ValueError(
                    f"REPLAY_BUILD_OPTIONS does not exist: {replay_build_options_path}"
                )
        elif not replay_build_date:
            raise ValueError(
                "A .bin REPLAY_INPUT requires REPLAY_BUILD_OPTIONS=<BuildOptions> "
                "or REPLAY_BUILD_DATE=<iso8601> for a full replay build"
            )
        return

    raise ValueError(
        "REPLAY_INPUT must be a .deb, a cix_flash_all.bin, or an extracted release "
        f"directory; got: {input_path}"
    )


def validate_iso8601(name: str, value: str) -> None:
    try:
        dt.datetime.fromisoformat(value)
    except ValueError as exc:
        raise ValueError(f"{name} must be a valid ISO-8601 timestamp; got: {value}") from exc


def image_manifest_commands(image: str) -> list[list[str]]:
    commands: list[list[str]] = []
    docker = shutil.which("docker")
    podman = shutil.which("podman")
    if docker:
        commands.append([docker, "manifest", "inspect", image])
    if podman:
        commands.append([podman, "manifest", "inspect", image])
        commands.append([podman, "manifest", "inspect", f"docker://{image}"])
    return commands


def validate_buildbox_image(image: str) -> None:
    commands = image_manifest_commands(image)
    if not commands:
        raise ValueError(
            "BUILDBOX_IMAGE validation needs docker or podman on PATH to inspect "
            f"the remote manifest for {image}"
        )

    failures: list[str] = []
    for command in commands:
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode == 0:
            return
        detail = (result.stderr or result.stdout).strip()
        failures.append(f"{' '.join(command)} -> {detail or f'exit {result.returncode}'}")

    joined = "\n".join(f"  - {line}" for line in failures)
    raise ValueError(
        f"BUILDBOX_IMAGE could not be resolved via manifest inspection: {image}\n{joined}"
    )


def run_validate(args: argparse.Namespace) -> int:
    try:
        if args.v is not None:
            require_choice("V", args.v, {"0", "1"})

        artefact_mode = args.artefact_mode
        if artefact_mode is not None:
            require_choice("ARTEFACT_MODE", artefact_mode, VALID_ARTEFACT_MODES)

        if args.firmware_target is not None:
            require_choice("FIRMWARE_TARGET", args.firmware_target, VALID_FIRMWARE_TARGETS)

        if args.validation_target is not None:
            require_choice(
                "VALIDATION_TARGET",
                args.validation_target,
                VALID_RAW_FIRMWARE_TARGETS,
            )

        if args.firmware_board is not None:
            require_choice("FIRMWARE_BOARD", args.firmware_board, VALID_BOARDS)

        if args.firmware_distro is not None:
            require_choice("FIRMWARE_DISTRO", args.firmware_distro, VALID_FIRMWARE_DISTROS)

        if args.firmware_validate_on_build is not None:
            normalize_bool(args.firmware_validate_on_build)

        if args.debug_on_uart3 is not None and args.debug_on_uart3 != "":
            normalize_bool(args.debug_on_uart3)
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "DEBUG_ON_UART3 is only supported with ARTEFACT_MODE=custom"
                )

        if args.uart3_enable is not None and args.uart3_enable != "":
            normalize_bool(args.uart3_enable)
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "UART3_ENABLE is only supported with ARTEFACT_MODE=custom"
                )

        if args.debug_verbose is not None and args.debug_verbose != "":
            normalize_bool(args.debug_verbose)
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "DEBUG_VERBOSE is only supported with ARTEFACT_MODE=custom"
                )

        if args.debug_print_error_level:
            validate_debug_print_error_level(
                "DEBUG_PRINT_ERROR_LEVEL", args.debug_print_error_level
            )
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "DEBUG_PRINT_ERROR_LEVEL is only supported with ARTEFACT_MODE=custom"
                )

        if (
            args.enable_firmware_fixes is not None
            and args.enable_firmware_fixes != ""
        ):
            firmware_fixes = normalize_bool(args.enable_firmware_fixes)
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "ENABLE_FIRMWARE_FIXES is only supported with ARTEFACT_MODE=custom"
                )
            if (
                firmware_fixes == "TRUE"
                and args.firmware_board is not None
                and args.firmware_board not in VALID_BOARDS
            ):
                raise ValueError(
                    "ENABLE_FIRMWARE_FIXES is only supported for FIRMWARE_BOARD=O6 or O6N"
                )

        if args.enable_core_order is not None and args.enable_core_order != "":
            core_order = args.enable_core_order.strip().lower()
            require_choice("ENABLE_CORE_ORDER", core_order, VALID_CORE_ORDERS)
            if core_order != "cix":
                if artefact_mode and artefact_mode != "custom":
                    raise ValueError(
                        "ENABLE_CORE_ORDER is only supported with ARTEFACT_MODE=custom"
                    )
                if normalize_bool(args.enable_firmware_fixes or "false") != "TRUE":
                    raise ValueError(
                        f"ENABLE_CORE_ORDER={core_order} requires ENABLE_FIRMWARE_FIXES=true"
                    )
                if (
                    args.firmware_board is not None
                    and args.firmware_board not in VALID_BOARDS
                ):
                    raise ValueError(
                        "ENABLE_CORE_ORDER is only supported for FIRMWARE_BOARD=O6 or O6N"
                    )

        if args.cix_release is not None and args.cix_release != "":
            normalized_cix_release = normalize_cix_release(args.cix_release)
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "CIX_RELEASE is only supported with ARTEFACT_MODE=custom"
                )
            if (
                normalized_cix_release == "1.2"
                and args.firmware_board is not None
                and args.firmware_board not in VALID_BOARDS
            ):
                raise ValueError(
                    "CIX_RELEASE is only supported for FIRMWARE_BOARD=O6 or O6N"
                )

        if (
            args.enable_experimental_uefi_settings is not None
            and args.enable_experimental_uefi_settings != ""
        ):
            experimental_uefi_settings = normalize_bool(
                args.enable_experimental_uefi_settings
            )
            if artefact_mode and artefact_mode != "custom":
                raise ValueError(
                    "ENABLE_EXPERIMENTAL_UEFI_SETTINGS is only supported with ARTEFACT_MODE=custom"
                )
            if (
                experimental_uefi_settings == "TRUE"
                and args.firmware_board is not None
                and args.firmware_board not in VALID_BOARDS
            ):
                raise ValueError(
                    "ENABLE_EXPERIMENTAL_UEFI_SETTINGS is only supported for FIRMWARE_BOARD=O6 or O6N"
                )

        if args.validation_profile:
            profile_file = args.profile_file.resolve()
            validate_profile(
                profile_file,
                args.validation_profile,
                args.validation_board,
                args.validation_target,
            )

        if args.replay_build_options:
            replay_build_options_path = pathlib.Path(args.replay_build_options).expanduser().resolve()
            if not replay_build_options_path.is_file():
                raise ValueError(
                    f"REPLAY_BUILD_OPTIONS does not exist: {replay_build_options_path}"
                )

        if args.replay_build_date:
            validate_iso8601("REPLAY_BUILD_DATE", args.replay_build_date)

        if args.replay_input:
            validate_replay_input(
                args.replay_input,
                args.firmware_board,
                args.replay_build_options,
                args.replay_build_date,
            )

        if args.buildbox_platform:
            require_choice(
                "BUILDBOX_PLATFORM",
                args.buildbox_platform,
                VALID_BUILDBOX_PLATFORMS,
            )

        if args.buildbox_image and args.check_buildbox_image:
            validate_buildbox_image(args.buildbox_image)
    except ValueError as exc:
        return fail(str(exc))

    return 0


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "normalize-bool":
        try:
            print(normalize_bool(args.value))
        except ValueError as exc:
            return fail(str(exc))
        return 0

    if args.command == "normalize-cix-release":
        try:
            print(normalize_cix_release(args.value))
        except ValueError as exc:
            return fail(str(exc))
        return 0

    if args.command == "normalize-u32":
        try:
            print(format_u32(parse_u32("value", args.value)))
        except ValueError as exc:
            return fail(str(exc))
        return 0

    if args.command == "describe-debug-print-error-level":
        try:
            parsed = validate_debug_print_error_level("value", args.value)
            print(describe_debug_print_error_level_value(parsed))
        except ValueError as exc:
            return fail(str(exc))
        return 0

    if args.command == "max-debug-print-error-level":
        try:
            print(format_u32(debug_print_error_level_mask()))
        except ValueError as exc:
            return fail(str(exc))
        return 0

    if args.command == "list-debug-print-error-level-bits":
        try:
            for name, value, comment in parse_debug_print_error_level_bits():
                if comment:
                    print(f"{format_u32(value)}  {name:<15}  {comment}")
                else:
                    print(f"{format_u32(value)}  {name}")
        except ValueError as exc:
            return fail(str(exc))
        return 0

    if args.command == "validate":
        return run_validate(args)

    return fail(f"Unknown command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
