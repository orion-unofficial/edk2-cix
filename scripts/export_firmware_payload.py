#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

from firmware_metadata_audit import audit_targets, format_findings


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent
BASETOOLS_GENFW = (
    REPO_ROOT / "src" / "edk2" / "BaseTools" / "Source" / "C" / "bin" / "GenFw"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Stage, install, or archive the deployable firmware payload without "
            "building a Debian package."
        )
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--repo-root", type=pathlib.Path, default=REPO_ROOT)
    common.add_argument("--board", default="O6")
    common.add_argument("--product", default="orion-o6")
    common.add_argument("--target", default="RELEASE_GCC5")
    common.add_argument(
        "--artefact-mode",
        choices=("custom", "upstream"),
        default=os.environ.get("ARTEFACT_MODE", "custom"),
    )
    common.add_argument("--version")

    stage = subparsers.add_parser("stage", parents=[common])
    stage.add_argument("--output-dir", type=pathlib.Path, required=True)

    install = subparsers.add_parser("install", parents=[common])
    install.add_argument("--install-root", type=pathlib.Path, required=True)

    zip_cmd = subparsers.add_parser("zip", parents=[common])
    zip_cmd.add_argument("--output", type=pathlib.Path, required=True)

    targz = subparsers.add_parser("targz", parents=[common])
    targz.add_argument("--output", type=pathlib.Path, required=True)

    return parser.parse_args()


def detect_version(repo_root: pathlib.Path, explicit: str | None) -> str:
    if explicit:
        return explicit
    try:
        result = subprocess.run(
            ["dpkg-parsechangelog", "-S", "Version"],
            cwd=repo_root,
            check=True,
            capture_output=True,
            text=True,
        )
        version = result.stdout.strip()
        if version:
            return version
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    changelog = repo_root / "debian" / "changelog"
    if changelog.is_file():
        first_line = changelog.read_text(encoding="utf-8").splitlines()[0]
        prefix = first_line.partition("(")[2]
        version = prefix.partition(")")[0].strip()
        if version:
            return version
    raise RuntimeError("Could not determine firmware version")


def copy_required_file(source: pathlib.Path, destination: pathlib.Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def parse_build_options(path: pathlib.Path) -> dict[str, str]:
    result: dict[str, str] = {}
    if not path.is_file():
        return result
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("Active Platform: "):
            result["active_platform"] = line.partition(": ")[2].strip()
        elif line.startswith("Flash Image Definition: "):
            result["flash_definition"] = line.partition(": ")[2].strip()
    return result


def resolve_genfw(repo_root: pathlib.Path) -> pathlib.Path:
    local_genfw = repo_root / "src" / BASETOOLS_GENFW.relative_to(REPO_ROOT / "src")
    if local_genfw.is_file():
        return local_genfw
    resolved = shutil.which("GenFw")
    if resolved:
        return pathlib.Path(resolved)
    raise RuntimeError(
        "GenFw was not found. Run `make -C src prepare-basetools-c` before staging a "
        "custom payload."
    )


def zero_debug_metadata(genfw: pathlib.Path, source: pathlib.Path, destination: pathlib.Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(genfw), "--zero", "-o", str(destination), str(source)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        stderr = result.stderr.strip() or result.stdout.strip() or "unknown GenFw failure"
        raise RuntimeError(f"Failed to zero debug metadata in {source}: {stderr}")


def resolve_repo_relative_board_file(
    repo_root: pathlib.Path,
    board: str,
    suffix: str,
) -> str:
    search_root = repo_root / "src" / "edk2-platforms" / "Platform" / "Radxa"
    candidates = sorted(search_root.rglob(f"{board}{suffix}"))
    if len(candidates) != 1:
        rendered = ", ".join(candidate.relative_to(repo_root).as_posix() for candidate in candidates)
        raise RuntimeError(
            f"Expected exactly one {board}{suffix} under {search_root}, found {len(candidates)}"
            + (f": {rendered}" if rendered else "")
        )
    return candidates[0].resolve().relative_to(repo_root.resolve()).as_posix()


def validate_custom_build_options(
    path: pathlib.Path,
    expected_active_platform: str,
    expected_flash_definition: str,
) -> None:
    parsed = parse_build_options(path)
    active_platform = parsed.get("active_platform")
    if active_platform != expected_active_platform:
        raise RuntimeError(
            f"Expected BuildOptions Active Platform to be {expected_active_platform}, got "
            f"{active_platform or 'missing'}"
        )
    flash_definition = parsed.get("flash_definition")
    if flash_definition != expected_flash_definition:
        raise RuntimeError(
            f"Expected BuildOptions Flash Image Definition to be {expected_flash_definition}, got "
            f"{flash_definition or 'missing'}"
        )


def audit_custom_payload(
    stage_root: pathlib.Path,
    build_dir: pathlib.Path,
    product: str,
    board: str,
    target: str,
) -> None:
    product_root = stage_root / product
    targets: list[tuple[str, pathlib.Path]] = []
    for path in sorted(product_root.glob("cix_flash*.bin")):
        targets.append((path.relative_to(stage_root).as_posix(), path))
    for path in sorted(product_root.glob("*.efi")):
        targets.append((path.relative_to(stage_root).as_posix(), path))
    build_options_path = product_root / "BuildOptions"
    if build_options_path.is_file():
        targets.append((build_options_path.relative_to(stage_root).as_posix(), build_options_path))

    bootloader3_path = build_dir / "Firmwares" / "bootloader3.img"
    targets.append((f"Build/{board}/{target}/Firmwares/bootloader3.img", bootloader3_path))

    findings = audit_targets(targets)
    if findings:
        rendered = "\n".join(f"  - {line}" for line in format_findings(findings))
        raise RuntimeError(
            "Refusing to stage the custom payload because shipped artefacts still expose "
            f"debug metadata or workspace breadcrumbs:\n{rendered}"
        )


def payload_mapping(
    repo_root: pathlib.Path, board: str, product: str, target: str
) -> list[tuple[pathlib.Path, pathlib.Path]]:
    build_dir = repo_root / "src" / "Build" / board / target
    flash_tool_dir = (
        repo_root / "src" / "edk2-non-osi" / "Platform" / "CIX" / "Sky1" / "FlashTool"
    )
    return [
        (repo_root / "src" / "scripts" / "welcome.nsh", pathlib.Path("startup.nsh")),
        (build_dir / "cix_flash_all.bin", pathlib.Path(product) / "cix_flash_all.bin"),
        (build_dir / "cix_flash_ota.bin", pathlib.Path(product) / "cix_flash_ota.bin"),
        (build_dir / "BuildOptions", pathlib.Path(product) / "BuildOptions"),
        (
            build_dir / "AARCH64" / "EnrollFromDefaultKeysApp.efi",
            pathlib.Path(product) / "EnrollFromDefaultKeysApp.efi",
        ),
        (
            build_dir / "AARCH64" / "VariableInfo.efi",
            pathlib.Path(product) / "VariableInfo.efi",
        ),
        (build_dir / "AARCH64" / "Shell.efi", pathlib.Path(product) / "Shell.efi"),
        (
            flash_tool_dir / "BurnImage.efi",
            pathlib.Path(product) / "BurnImage.efi",
        ),
        (
            flash_tool_dir / "FlashUpdate.efi",
            pathlib.Path(product) / "FlashUpdate.efi",
        ),
        (repo_root / "src" / "scripts" / "startup.nsh", pathlib.Path(product) / "startup.nsh"),
    ]


def stage_payload(
    repo_root: pathlib.Path,
    board: str,
    product: str,
    target: str,
    artefact_mode: str,
    output_dir: pathlib.Path,
) -> pathlib.Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    build_dir = repo_root / "src" / "Build" / board / target
    expected_active_platform = resolve_repo_relative_board_file(repo_root, board, ".dsc")
    expected_flash_definition = resolve_repo_relative_board_file(repo_root, board, ".fdf")
    genfw = resolve_genfw(repo_root) if artefact_mode == "custom" else None

    for source, relative_destination in payload_mapping(repo_root, board, product, target):
        destination = output_dir / relative_destination
        if artefact_mode == "custom" and source.name in {"BurnImage.efi", "FlashUpdate.efi"}:
            assert genfw is not None
            zero_debug_metadata(genfw, source, destination)
        else:
            copy_required_file(source, destination)

    if artefact_mode == "custom":
        validate_custom_build_options(
            output_dir / product / "BuildOptions",
            expected_active_platform,
            expected_flash_definition,
        )
        audit_custom_payload(output_dir, build_dir, product, board, target)
    return output_dir


def install_payload(
    repo_root: pathlib.Path,
    board: str,
    product: str,
    target: str,
    artefact_mode: str,
    install_root: pathlib.Path,
    version: str,
) -> pathlib.Path:
    destination = install_root / version
    if destination.exists():
        shutil.rmtree(destination)
    with tempfile.TemporaryDirectory(prefix=f"{product}-{version}-stage-") as tmpdir_text:
        stage_dir = pathlib.Path(tmpdir_text) / version
        stage_payload(repo_root, board, product, target, artefact_mode, stage_dir)
        try:
            shutil.copytree(stage_dir, destination)
        except PermissionError as exc:
            raise SystemExit(
                f"Cannot install firmware payload to {destination}: {exc.strerror}. "
                "Use the stage/zip/targz commands for packaging workflows, or pass "
                "--install-root to a writable live-system mount such as "
                "/boot/efi/firmware/radxa."
            ) from exc
    return destination


def archive_root_path(product: str, version: str) -> pathlib.Path:
    return pathlib.Path(product) / version


def create_zip(
    repo_root: pathlib.Path,
    board: str,
    product: str,
    target: str,
    artefact_mode: str,
    output_path: pathlib.Path,
    version: str,
) -> pathlib.Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{product}-{version}-zip-") as tmpdir_text:
        tmpdir = pathlib.Path(tmpdir_text)
        stage_base = tmpdir / archive_root_path(product, version)
        stage_payload(repo_root, board, product, target, artefact_mode, stage_base)
        with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage_base.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(tmpdir))
    return output_path


def create_targz(
    repo_root: pathlib.Path,
    board: str,
    product: str,
    target: str,
    artefact_mode: str,
    output_path: pathlib.Path,
    version: str,
) -> pathlib.Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{product}-{version}-tar-") as tmpdir_text:
        tmpdir = pathlib.Path(tmpdir_text)
        stage_base = tmpdir / archive_root_path(product, version)
        stage_payload(repo_root, board, product, target, artefact_mode, stage_base)
        with tarfile.open(output_path, "w:gz") as archive:
            archive.add(stage_base.parent, arcname=stage_base.parent.relative_to(tmpdir).as_posix())
    return output_path


def main() -> None:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    version = detect_version(repo_root, args.version)
    suppress_output = os.environ.get("EDK2_CIX_SUPPRESS_EXPORT_OUTPUT", "0") == "1"

    if args.command == "stage":
        destination = stage_payload(
            repo_root,
            args.board,
            args.product,
            args.target,
            args.artefact_mode,
            args.output_dir.resolve(),
        )
        if not suppress_output:
            print(destination)
    elif args.command == "install":
        destination = install_payload(
            repo_root,
            args.board,
            args.product,
            args.target,
            args.artefact_mode,
            args.install_root.resolve(),
            version,
        )
        if not suppress_output:
            print(destination)
    elif args.command == "zip":
        destination = create_zip(
            repo_root,
            args.board,
            args.product,
            args.target,
            args.artefact_mode,
            args.output.resolve(),
            version,
        )
        if not suppress_output:
            print(destination)
    elif args.command == "targz":
        destination = create_targz(
            repo_root,
            args.board,
            args.product,
            args.target,
            args.artefact_mode,
            args.output.resolve(),
            version,
        )
        if not suppress_output:
            print(destination)


if __name__ == "__main__":
    main()
