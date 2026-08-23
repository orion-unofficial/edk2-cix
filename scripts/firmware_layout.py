#!/usr/bin/env python3

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path


VERSION_FILENAME = "VERSION"
MAX_DISPLAY_VERSION_LENGTH = 48
VALID_ARTEFACT_MODES = {"custom", "upstream"}
VALID_CIX_RELEASES = {"", "1.2"}
VALID_CORE_ORDERS = {"", "cix", "conventional", "performance"}
VALID_FIRMWARE_TARGETS = {"RELEASE", "DEBUG"}
PATH_ROOT = ("edk2", "radxa")


def _parse_bool(value: str | bool | None) -> bool:
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    normalized = value.strip().lower()
    if not normalized:
        return False
    if normalized in {"1", "true", "yes", "y", "on"}:
        return True
    if normalized in {"0", "false", "no", "n", "off"}:
        return False
    raise ValueError(f"Unsupported boolean value: {value}")


def _normalize_core_order(value: str | None) -> str:
    if value is None:
        return ""
    normalized = value.strip().lower()
    if normalized not in VALID_CORE_ORDERS:
        raise ValueError(f"Unsupported core-order value: {value}")
    return normalized


def _normalize_cix_release(value: str | None) -> str:
    if value is None:
        return ""
    normalized = value.strip().lower()
    if normalized == "v1.2":
        normalized = "1.2"
    if normalized not in VALID_CIX_RELEASES:
        raise ValueError(f"Unsupported CIX release value: {value}")
    return normalized


def _leaf_token(name: str) -> str:
    replacements = {
        "experimental_uefi_settings": "experimental",
        "uart3_enable": "uart3",
        "debug_on_uart3": "uart3_debug",
        "debug_verbose": "verbose",
    }
    return replacements.get(name, name)


def _normalize_target(value: str) -> str:
    normalized = value.strip().upper()
    if normalized.endswith("_GCC5"):
        normalized = normalized[: -len("_GCC5")]
    if normalized not in VALID_FIRMWARE_TARGETS:
        raise ValueError(f"Unsupported firmware target: {value}")
    return normalized


def _normalize_u32_literal(value: str | None) -> str:
    if value is None:
        return ""
    normalized = value.strip().lower()
    if not normalized:
        return ""
    parsed = int(normalized, 0)
    if parsed < 0 or parsed > 0xFFFFFFFF:
        raise ValueError(f"Unsupported 32-bit value: {value}")
    return f"0x{parsed:08x}"


@dataclass(frozen=True)
class FirmwareLayout:
    artefact_mode: str = "custom"
    firmware_target: str = "RELEASE"
    enable_firmware_fixes: bool = False
    enable_core_order: str = ""
    cix_release: str = ""
    enable_tf_a_fixes: bool = False
    enable_experimental_uefi_settings: bool = False
    debug_on_uart3: bool = False
    uart3_enable: bool = False
    debug_verbose: bool = False
    debug_print_error_level: str = ""

    def __post_init__(self) -> None:
        artefact_mode = self.artefact_mode.strip().lower()
        if artefact_mode not in VALID_ARTEFACT_MODES:
            raise ValueError(f"Unsupported artefact mode: {self.artefact_mode}")
        firmware_target = _normalize_target(self.firmware_target)
        enable_core_order = _normalize_core_order(self.enable_core_order)
        cix_release = _normalize_cix_release(self.cix_release)
        debug_print_error_level = _normalize_u32_literal(self.debug_print_error_level)
        object.__setattr__(self, "artefact_mode", artefact_mode)
        object.__setattr__(self, "firmware_target", firmware_target)
        object.__setattr__(self, "enable_core_order", enable_core_order)
        object.__setattr__(self, "cix_release", cix_release)
        object.__setattr__(self, "debug_print_error_level", debug_print_error_level)

        if artefact_mode != "custom":
            if any(
                (
                    self.enable_firmware_fixes,
                    bool(enable_core_order),
                    bool(cix_release),
                    self.enable_tf_a_fixes,
                    self.enable_experimental_uefi_settings,
                    self.debug_on_uart3,
                    self.uart3_enable,
                    self.debug_verbose,
                    bool(debug_print_error_level),
                )
            ):
                raise ValueError("Custom-only build flags are not supported in upstream mode")
        if enable_core_order and enable_core_order != "cix" and not self.enable_firmware_fixes:
            raise ValueError("Non-default core-order selections require ENABLE_FIRMWARE_FIXES=true")

    @property
    def effective_uart3_enable(self) -> bool:
        return self.uart3_enable or self.debug_on_uart3

    @property
    def effective_debug_verbose(self) -> bool:
        return self.artefact_mode == "custom" and self.firmware_target == "RELEASE" and self.debug_verbose

    @property
    def effective_core_order(self) -> str:
        if not self.enable_firmware_fixes:
            return ""
        return self.enable_core_order

    def leaf_parts(self) -> tuple[str, ...]:
        if self.artefact_mode == "upstream":
            if self.firmware_target == "DEBUG":
                return ("upstream", "debug")
            return ()

        parts = ["custom"]
        if self.cix_release:
            parts.append("cix")
            if self.enable_tf_a_fixes:
                parts.append("tf_a_fixes")
        if self.firmware_target == "DEBUG":
            parts.append("debug")
        if self.enable_firmware_fixes:
            parts.append("fixes")
            if self.effective_core_order in {"conventional", "performance"}:
                parts.extend(("core_order", self.effective_core_order))
        if self.enable_experimental_uefi_settings:
            parts.append(_leaf_token("experimental_uefi_settings"))
        if self.effective_uart3_enable:
            parts.append(_leaf_token("uart3_enable"))
            if self.debug_on_uart3:
                parts.append(_leaf_token("debug_on_uart3"))
        if self.effective_debug_verbose:
            parts.append(_leaf_token("debug_verbose"))
        if self.debug_print_error_level:
            parts.extend(("debug_print_error_level", self.debug_print_error_level))
        return tuple(parts)

    def leaf_path(self) -> Path:
        parts = self.leaf_parts()
        return Path(*parts) if parts else Path()

    def archive_suffix_tokens(self) -> list[str]:
        tokens: list[str] = []
        if self.artefact_mode == "custom":
            tokens.append("custom")
        if self.cix_release:
            tokens.append("cix")
            if self.enable_tf_a_fixes:
                tokens.append("tf_a_fixes")
        if self.firmware_target == "DEBUG":
            tokens.append("debug")
        if self.enable_firmware_fixes:
            tokens.append("fixes")
            if self.effective_core_order in {"conventional", "performance"}:
                tokens.append(f"core_order-{self.effective_core_order}")
        if self.enable_experimental_uefi_settings:
            tokens.append(_leaf_token("experimental_uefi_settings"))
        if self.effective_uart3_enable:
            tokens.append(_leaf_token("uart3_enable"))
            if self.debug_on_uart3:
                tokens.append(_leaf_token("debug_on_uart3"))
        if self.effective_debug_verbose:
            tokens.append(_leaf_token("debug_verbose"))
        if self.debug_print_error_level:
            tokens.append(f"debug_print_error_level-{self.debug_print_error_level}")
        return tokens

    def archive_suffix(self) -> str:
        return "+".join(self.archive_suffix_tokens())

    def _display_feature_tokens(self) -> list[str]:
        tokens: list[str] = []
        if self.cix_release:
            tokens.append("cix")
            if self.enable_tf_a_fixes:
                tokens.append("tf_a_fixes")
        if self.enable_firmware_fixes:
            tokens.append("fixes")
            if self.effective_core_order == "conventional":
                tokens.append("core_order_conv")
            elif self.effective_core_order == "performance":
                tokens.append("core_order_perf")
        if self.enable_experimental_uefi_settings:
            tokens.append("experimental")
        if self.debug_on_uart3:
            tokens.append("uart3_debug")
        elif self.effective_uart3_enable:
            tokens.append("uart3")
        if self.effective_debug_verbose:
            tokens.append("verbose")
        if self.debug_print_error_level:
            tokens.append(f"mask{self.debug_print_error_level.removeprefix('0x')}")
        if self.artefact_mode == "custom" and not tokens:
            tokens.append("custom")
        return tokens


def version_file(repo_root: Path) -> Path:
    return repo_root / VERSION_FILENAME


def read_version(repo_root: Path) -> str:
    path = version_file(repo_root)
    if not path.is_file():
        raise FileNotFoundError(f"Missing firmware version file: {path}")
    version = path.read_text(encoding="utf-8").strip()
    if not version:
        raise ValueError(f"Firmware version file is empty: {path}")
    return version


def archive_root_path(product: str, version: str, layout: FirmwareLayout) -> Path:
    root = Path(*PATH_ROOT) / product / version
    leaf = layout.leaf_path()
    return root / leaf if leaf.parts else root


def _shorten_token(token: str, phase: int) -> str:
    phase_one = {
        "cix": "cix",
        "tf_a_fixes": "tfa_fixes",
        "uart3_debug": "u3_dbg",
        "uart3": "u3",
        "verbose": "verb",
        "experimental": "exp",
        "core_order_conv": "co_conv",
        "core_order_perf": "co_perf",
        "custom": "cust",
        "fixes": "fixes",
    }
    phase_two = {
        "cix": "cix",
        "tf_a_fixes": "tfaf",
        "uart3_debug": "u3d",
        "uart3": "u3",
        "verbose": "v",
        "experimental": "exp",
        "core_order_conv": "coc",
        "core_order_perf": "cop",
        "custom": "c",
        "fixes": "fx",
    }
    if token.startswith("mask"):
        return "mask" if phase >= 1 else token
    if phase == 0:
        return token
    if phase == 1:
        return phase_one.get(token, token)
    return phase_two.get(token, phase_one.get(token, token))


def display_version(
    version: str,
    layout: FirmwareLayout,
    source_commit_hash: str = "",
) -> str:
    raw_tokens = layout._display_feature_tokens()
    debug_hash = source_commit_hash.strip().lower()

    def render(phase: int, hash_length: int | None) -> str:
        tokens = [_shorten_token(token, phase) for token in raw_tokens]
        rendered = version
        if tokens:
            rendered += "+" + "+".join(tokens)
        if layout.firmware_target == "DEBUG":
            suffix = "-debug"
            if debug_hash:
                effective_hash = debug_hash[:hash_length] if hash_length is not None else debug_hash
                suffix += f"+{effective_hash}"
            rendered += suffix
        return rendered

    for phase in range(3):
        candidate = render(phase, None)
        if len(candidate) <= MAX_DISPLAY_VERSION_LENGTH:
            return candidate

    for hash_length in (10, 8, 7):
        candidate = render(2, hash_length)
        if len(candidate) <= MAX_DISPLAY_VERSION_LENGTH:
            return candidate

    raise ValueError(
        "Could not derive a firmware display version within the 48-character budget"
    )


def read_debian_changelog_version(repo_root: Path) -> str:
    changelog = repo_root / "debian" / "changelog"
    if not changelog.is_file():
        raise FileNotFoundError(f"Missing Debian changelog: {changelog}")
    first_line = changelog.read_text(encoding="utf-8").splitlines()[0]
    start = first_line.find("(")
    end = first_line.find(")", start + 1)
    if start == -1 or end == -1:
        raise ValueError(f"Could not parse Debian changelog version from: {first_line}")
    version = first_line[start + 1 : end].strip()
    if not version:
        raise ValueError(f"Debian changelog version is empty in: {first_line}")
    return version


def debian_upstream_version(version: str) -> str:
    without_epoch = version.split(":", 1)[-1]
    if "-" in without_epoch:
        return without_epoch.rsplit("-", 1)[0]
    return without_epoch


def validate_debian_version(repo_root: Path) -> tuple[str, str]:
    firmware_version = read_version(repo_root)
    debian_version = read_debian_changelog_version(repo_root)
    debian_upstream = debian_upstream_version(debian_version)
    if debian_upstream != firmware_version:
        raise ValueError(
            "Debian changelog version does not match VERSION: "
            f"{debian_upstream} != {firmware_version}"
        )
    return firmware_version, debian_version


def _iter_custom_target_variants(target: str) -> list[FirmwareLayout]:
    generated: list[FirmwareLayout] = []
    cix_release_options = (("", False), ("1.2", False), ("1.2", True))
    presets = [
        {},
        {"enable_firmware_fixes": True},
        {"enable_firmware_fixes": True, "enable_core_order": "conventional"},
        {"enable_firmware_fixes": True, "enable_core_order": "performance"},
        {"enable_experimental_uefi_settings": True},
        {"uart3_enable": True},
        {"uart3_enable": True, "debug_on_uart3": True},
    ]
    if target == "RELEASE":
        presets.insert(5, {"debug_verbose": True})

    # Keep broad compile invalidators ahead of the UART/debug leaves, then flip
    # the narrower curated-CIX path inside each preset bucket.
    for preset in presets:
        for cix_release, enable_tf_a_fixes in cix_release_options:
            generated.append(
                FirmwareLayout(
                    artefact_mode="custom",
                    firmware_target=target,
                    cix_release=cix_release,
                    enable_tf_a_fixes=enable_tf_a_fixes,
                    **preset,
                )
            )
    return generated


def iter_build_all_variants() -> list[FirmwareLayout]:
    variants = [FirmwareLayout(artefact_mode="upstream", firmware_target="RELEASE")]
    variants.extend(_iter_custom_target_variants("RELEASE"))
    variants.extend(_iter_custom_target_variants("DEBUG"))
    return variants


def parse_layout_args(args: argparse.Namespace) -> FirmwareLayout:
    return FirmwareLayout(
        artefact_mode=args.artefact_mode,
        firmware_target=args.firmware_target,
        enable_firmware_fixes=_parse_bool(getattr(args, "enable_firmware_fixes", None)),
        enable_core_order=getattr(args, "enable_core_order", ""),
        cix_release=getattr(args, "cix_release", ""),
        enable_tf_a_fixes=_parse_bool(getattr(args, "enable_tf_a_fixes", None)),
        enable_experimental_uefi_settings=_parse_bool(
            getattr(args, "enable_experimental_uefi_settings", None)
        ),
        debug_on_uart3=_parse_bool(getattr(args, "debug_on_uart3", None)),
        uart3_enable=_parse_bool(getattr(args, "uart3_enable", None)),
        debug_verbose=_parse_bool(getattr(args, "debug_verbose", None)),
        debug_print_error_level=getattr(args, "debug_print_error_level", ""),
    )


def add_layout_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--artefact-mode", default="custom")
    parser.add_argument("--firmware-target", default="RELEASE")
    parser.add_argument("--enable-firmware-fixes")
    parser.add_argument("--enable-core-order")
    parser.add_argument("--cix-release")
    parser.add_argument("--enable-tf-a-fixes")
    parser.add_argument("--enable-experimental-uefi-settings")
    parser.add_argument("--debug-on-uart3")
    parser.add_argument("--uart3-enable")
    parser.add_argument("--debug-verbose")
    parser.add_argument("--debug-print-error-level")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Resolve firmware version, layout, and naming metadata."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    version_parser = subparsers.add_parser("version")
    version_parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)

    leaf_parser = subparsers.add_parser("leaf-path")
    add_layout_args(leaf_parser)

    suffix_parser = subparsers.add_parser("archive-suffix")
    add_layout_args(suffix_parser)

    display_parser = subparsers.add_parser("display-version")
    display_parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)
    display_parser.add_argument("--version")
    display_parser.add_argument("--source-commit-hash", default="")
    add_layout_args(display_parser)

    validate_parser = subparsers.add_parser("validate-debian-version")
    validate_parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parent.parent
    )

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.command == "version":
        print(read_version(args.repo_root.resolve()))
        return 0

    if args.command == "validate-debian-version":
        firmware_version, debian_version = validate_debian_version(args.repo_root.resolve())
        print(f"VERSION={firmware_version}")
        print(f"DEBIAN_VERSION={debian_version}")
        return 0

    layout = parse_layout_args(args)
    if args.command == "leaf-path":
        print(layout.leaf_path().as_posix() if layout.leaf_path().parts else "")
        return 0
    if args.command == "archive-suffix":
        print(layout.archive_suffix())
        return 0
    if args.command == "display-version":
        version = args.version or read_version(args.repo_root.resolve())
        print(display_version(version, layout, args.source_commit_hash))
        return 0
    raise AssertionError(f"Unhandled command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
