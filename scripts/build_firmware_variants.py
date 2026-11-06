#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import textwrap
import time
import zipfile

import firmware_layout


SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
CUSTOM_DISTRO = "trixie"
UPSTREAM_DISTRO = "trixie"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build and archive all supported firmware variants."
    )
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--board", default="O6")
    parser.add_argument("--product", default="orion-o6")
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--archive-format", choices=("zip", "targz"), required=True)
    parser.add_argument("--make", default="make")
    parser.add_argument("--debug-print-error-level", default="")
    parser.add_argument("--ccache-dir", default=os.environ.get("CCACHE_DIR", ""))
    parser.add_argument("--ccache-wrapper-root", default=os.environ.get("CCACHE_WRAPPER_ROOT", ""))
    parser.add_argument("--ccache-bin", default=os.environ.get("CCACHE_BIN", ""))
    parser.add_argument("--ccache-disable", default=None)
    parser.add_argument("--v", default=os.environ.get("V", "0"))
    return parser.parse_args()


def default_distro_for_layout(layout: firmware_layout.FirmwareLayout) -> str:
    if layout.artefact_mode == "upstream":
        return UPSTREAM_DISTRO
    return CUSTOM_DISTRO


def effective_distro(layout: firmware_layout.FirmwareLayout) -> str:
    return default_distro_for_layout(layout)


def phase_label(layout: firmware_layout.FirmwareLayout) -> str:
    if layout.artefact_mode == "upstream":
        return "vendor"

    parts = ["custom"]
    if layout.cix_release:
        parts.append("cix")
        if layout.enable_tf_a_fixes:
            parts.append("tf_a_fixes")
    if layout.firmware_target == "DEBUG":
        parts.append("debug")
    return "/".join(parts)


def traversal_group_label(layout: firmware_layout.FirmwareLayout) -> str:
    if layout.artefact_mode == "upstream":
        return "vendor"

    parts = ["custom"]
    if layout.firmware_target == "DEBUG":
        parts.append("debug")
    return "/".join(parts)


def variant_label(layout: firmware_layout.FirmwareLayout) -> str:
    leaf = layout.leaf_path().as_posix()
    return "vendor" if leaf in {"", "."} else leaf


def variant_options_label(layout: firmware_layout.FirmwareLayout) -> str:
    return layout.archive_suffix() or "vendor"


def variant_make_args(
    layout: firmware_layout.FirmwareLayout,
    board: str,
    product: str,
    version: str,
    stage_root: Path,
    distro: str,
    debug_print_error_level: str = "",
    ccache_dir: str = "",
    ccache_wrapper_root: str = "",
    ccache_bin: str = "",
    ccache_disable: str | None = None,
) -> list[str]:
    args = [
        f"ARTEFACT_MODE={layout.artefact_mode}",
        f"FIRMWARE_TARGET={layout.firmware_target}",
        f"FIRMWARE_BOARD={board}",
        f"FIRMWARE_PRODUCT={product}",
        f"FIRMWARE_VERSION={version}",
        f"FIRMWARE_STAGE_ROOT={stage_root}",
        f"FIRMWARE_DISTRO={distro}",
        "EDK2_CIX_SUPPRESS_FINAL_OUTPUT=1",
        "EDK2_CIX_SUPPRESS_EXPORT_OUTPUT=1",
        "EDK2_CIX_SUPPRESS_BUILD_BANNER=1",
        f"ENABLE_FIRMWARE_FIXES={'TRUE' if layout.enable_firmware_fixes else ''}",
        f"ENABLE_CORE_ORDER={layout.effective_core_order or ''}",
        f"CIX_RELEASE={layout.cix_release or ''}",
        f"ENABLE_TF_A_FIXES={'TRUE' if layout.enable_tf_a_fixes else ''}",
        f"ENABLE_EXPERIMENTAL_UEFI_SETTINGS={'TRUE' if layout.enable_experimental_uefi_settings else ''}",
        f"DEBUG_ON_UART3={'TRUE' if layout.debug_on_uart3 else ''}",
        f"UART3_ENABLE={'TRUE' if layout.effective_uart3_enable else ''}",
        f"DEBUG_VERBOSE={'TRUE' if layout.debug_verbose else ''}",
    ]
    if ccache_dir:
        args.append(f"CCACHE_DIR={ccache_dir}")
    if ccache_wrapper_root:
        args.append(f"CCACHE_WRAPPER_ROOT={ccache_wrapper_root}")
    if ccache_bin:
        args.append(f"CCACHE_BIN={ccache_bin}")
    if ccache_disable is not None:
        args.append(f"CCACHE_DISABLE={ccache_disable}")
    if layout.artefact_mode == "custom":
        args.append("EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE=1")
        args.append(f"DEBUG_PRINT_ERROR_LEVEL={debug_print_error_level}")
    else:
        args.append("EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE=")
        args.append("DEBUG_PRINT_ERROR_LEVEL=")
    return args


def render_variant_summary(prefix: str, labels: list[str]) -> str:
    body = ", ".join(labels) if labels else "(none)"
    return textwrap.fill(
        f"{prefix}{body}",
        width=100,
        subsequent_indent=" " * len(prefix),
    )


def format_duration(seconds: float) -> str:
    total_seconds = max(0, int(round(seconds)))
    minutes, remaining_seconds = divmod(total_seconds, 60)
    hours, remaining_minutes = divmod(minutes, 60)
    if hours:
        return f"{hours}h {remaining_minutes}m {remaining_seconds}s"
    if remaining_minutes:
        return f"{remaining_minutes}m {remaining_seconds}s"
    return f"{remaining_seconds}s"


def fetch_ccache_stats(
    repo_root: Path,
    make_executable: str,
    verbosity: str,
    board: str,
    product: str,
    version: str,
    layout: firmware_layout.FirmwareLayout,
    distro: str,
    ccache_dir: str,
    ccache_wrapper_root: str,
    ccache_bin: str,
    ccache_disable: str | None,
) -> dict[str, int]:
    command = [
        make_executable,
        "--no-print-directory",
        "buildbox-ccache-stats",
        "CCACHE_STATS_FORMAT=json",
        f"V={verbosity}",
        f"ARTEFACT_MODE={layout.artefact_mode}",
        f"FIRMWARE_TARGET={layout.firmware_target}",
        f"FIRMWARE_BOARD={board}",
        f"FIRMWARE_PRODUCT={product}",
        f"FIRMWARE_VERSION={version}",
        f"FIRMWARE_DISTRO={distro}",
    ]
    if ccache_dir:
        command.append(f"CCACHE_DIR={ccache_dir}")
    if ccache_wrapper_root:
        command.append(f"CCACHE_WRAPPER_ROOT={ccache_wrapper_root}")
    if ccache_bin:
        command.append(f"CCACHE_BIN={ccache_bin}")
    if ccache_disable is not None:
        command.append(f"CCACHE_DISABLE={ccache_disable}")
    result = subprocess.run(
        command,
        cwd=repo_root,
        check=True,
        capture_output=True,
        text=True,
    )
    output = result.stdout.strip()
    if not output:
        return {}
    return json.loads(output)


def ccache_delta(before: dict[str, int], after: dict[str, int]) -> dict[str, int]:
    keys = {
        "cache_miss",
        "direct_cache_hit",
        "preprocessed_cache_hit",
        "cache_size_kibibyte",
    }
    return {
        key: max(0, int(after.get(key, 0)) - int(before.get(key, 0)))
        for key in keys
    }


def summarize_ccache_delta(before: dict[str, int], after: dict[str, int]) -> str:
    delta = ccache_delta(before, after)
    hits = delta["direct_cache_hit"] + delta["preprocessed_cache_hit"]
    misses = delta["cache_miss"]
    cacheable = hits + misses
    hit_rate = (hits / cacheable * 100.0) if cacheable else 0.0
    size_delta_mib = delta["cache_size_kibibyte"] / 1024.0
    return (
        f"{cacheable} cacheable, {hits} hits "
        f"({delta['direct_cache_hit']} direct, {delta['preprocessed_cache_hit']} preprocessed), "
        f"{misses} misses, {hit_rate:.1f}% hit rate, +{size_delta_mib:.1f} MiB"
    )


def build_variant(
    repo_root: Path,
    make_executable: str,
    verbosity: str,
    board: str,
    product: str,
    version: str,
    layout: firmware_layout.FirmwareLayout,
    variant_root: Path,
    debug_print_error_level: str,
    index: int,
    total: int,
    labels: list[str],
    previous_ccache_stats: dict[str, int] | None,
    build_all_started_at: float,
    ccache_dir: str,
    ccache_wrapper_root: str,
    ccache_bin: str,
    ccache_disable: str | None,
) -> dict[str, int] | None:
    stage_root = variant_root / firmware_layout.archive_root_path(product, version, layout)
    distro = effective_distro(layout)
    label = variant_options_label(layout)
    command = [
        make_executable,
        "--no-print-directory",
        "buildbox-firmware-stage",
        f"V={verbosity}",
        *variant_make_args(
            layout,
            board,
            product,
            version,
            stage_root,
            distro,
            debug_print_error_level,
            ccache_dir,
            ccache_wrapper_root,
            ccache_bin,
            ccache_disable,
        ),
    ]
    started_at = time.monotonic()
    print(
        f"[build-all] Building variant {index}/{total}: "
        f"{variant_label(layout)} ({distro}, {label})",
        flush=True,
    )
    subprocess.run(command, cwd=repo_root, check=True)
    duration = time.monotonic() - started_at
    elapsed = time.monotonic() - build_all_started_at
    current_ccache_stats = previous_ccache_stats
    ccache_summary = None
    try:
        current_ccache_stats = fetch_ccache_stats(
            repo_root=repo_root,
            make_executable=make_executable,
            verbosity=verbosity,
            board=board,
            product=product,
            version=version,
            layout=layout,
            distro=distro,
            ccache_dir=ccache_dir,
            ccache_wrapper_root=ccache_wrapper_root,
            ccache_bin=ccache_bin,
            ccache_disable=ccache_disable,
        )
    except (subprocess.CalledProcessError, json.JSONDecodeError):
        ccache_summary = "unavailable"
    else:
        if previous_ccache_stats is None:
            ccache_summary = summarize_ccache_delta({}, current_ccache_stats)
        else:
            ccache_summary = summarize_ccache_delta(previous_ccache_stats, current_ccache_stats)
    completed = labels[:index]
    pending = labels[index:]
    print()
    print(
        f"[build-all] Completed variant {index}/{total}: "
        f"{variant_label(layout)} ({distro}, {label}) in {format_duration(duration)} "
        f"(elapsed {format_duration(elapsed)})"
    )
    if ccache_summary is not None:
        print(f"[build-all] ccache: {ccache_summary}")
    print(render_variant_summary("[build-all] Completed: ", completed))
    print(render_variant_summary("[build-all] Pending:   ", pending))
    print()
    return current_ccache_stats


def create_zip(output_path: Path, source_root: Path) -> None:
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source_root.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(source_root.parent))


def create_targz(output_path: Path, source_root: Path) -> None:
    with tarfile.open(output_path, "w:gz") as archive:
        archive.add(source_root, arcname=source_root.name)


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    output_path = args.output.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    variants = firmware_layout.iter_build_all_variants()
    labels = [variant_label(layout) for layout in variants]
    temp_root = repo_root / ".buildbox" / "tmp"
    temp_root.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(
        dir=temp_root,
        prefix=f"{args.product}-{args.version}-build-all-",
    ) as tmpdir_text:
        variant_root = Path(tmpdir_text)
        build_all_started_at = time.monotonic()
        active_phase: tuple[str, str] | None = None
        previous_ccache_stats: dict[str, int] | None = None
        initial_layout = variants[0]
        try:
            previous_ccache_stats = fetch_ccache_stats(
                repo_root=repo_root,
                make_executable=args.make,
                verbosity=args.v,
                board=args.board,
                product=args.product,
                version=args.version,
                layout=initial_layout,
                distro=effective_distro(initial_layout),
                ccache_dir=args.ccache_dir,
                ccache_wrapper_root=args.ccache_wrapper_root,
                ccache_bin=args.ccache_bin,
                ccache_disable=args.ccache_disable,
            )
        except (subprocess.CalledProcessError, json.JSONDecodeError):
            previous_ccache_stats = None
        for index, layout in enumerate(variants, start=1):
            phase = (
                traversal_group_label(layout),
                effective_distro(layout),
            )
            if phase != active_phase:
                print(f"[build-all] Entering {phase[0]} group in {phase[1]}")
                active_phase = phase
            next_ccache_stats = build_variant(
                repo_root=repo_root,
                make_executable=args.make,
                verbosity=args.v,
                board=args.board,
                product=args.product,
                version=args.version,
                layout=layout,
                variant_root=variant_root,
                debug_print_error_level=args.debug_print_error_level.strip(),
                index=index,
                total=len(variants),
                labels=labels,
                previous_ccache_stats=previous_ccache_stats,
                build_all_started_at=build_all_started_at,
                ccache_dir=args.ccache_dir,
                ccache_wrapper_root=args.ccache_wrapper_root,
                ccache_bin=args.ccache_bin,
                ccache_disable=args.ccache_disable,
            )
            previous_ccache_stats = next_ccache_stats

        source_root = variant_root / "edk2"
        if args.archive_format == "zip":
            create_zip(output_path, source_root)
        else:
            create_targz(output_path, source_root)

    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
