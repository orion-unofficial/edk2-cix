#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


SCRIPT_PATH = pathlib.Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent


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
    output_dir: pathlib.Path,
) -> pathlib.Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    for source, relative_destination in payload_mapping(repo_root, board, product, target):
        copy_required_file(source, output_dir / relative_destination)
    return output_dir


def install_payload(
    repo_root: pathlib.Path,
    board: str,
    product: str,
    target: str,
    install_root: pathlib.Path,
    version: str,
) -> pathlib.Path:
    destination = install_root / version
    if destination.exists():
        shutil.rmtree(destination)
    with tempfile.TemporaryDirectory(prefix=f"{product}-{version}-stage-") as tmpdir_text:
        stage_dir = pathlib.Path(tmpdir_text) / version
        stage_payload(repo_root, board, product, target, stage_dir)
        shutil.copytree(stage_dir, destination)
    return destination


def archive_root_path(product: str, version: str) -> pathlib.Path:
    return pathlib.Path(product) / version


def create_zip(
    repo_root: pathlib.Path,
    board: str,
    product: str,
    target: str,
    output_path: pathlib.Path,
    version: str,
) -> pathlib.Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{product}-{version}-zip-") as tmpdir_text:
        tmpdir = pathlib.Path(tmpdir_text)
        stage_base = tmpdir / archive_root_path(product, version)
        stage_payload(repo_root, board, product, target, stage_base)
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
    output_path: pathlib.Path,
    version: str,
) -> pathlib.Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{product}-{version}-tar-") as tmpdir_text:
        tmpdir = pathlib.Path(tmpdir_text)
        stage_base = tmpdir / archive_root_path(product, version)
        stage_payload(repo_root, board, product, target, stage_base)
        with tarfile.open(output_path, "w:gz") as archive:
            archive.add(stage_base.parent, arcname=stage_base.parent.relative_to(tmpdir).as_posix())
    return output_path


def main() -> None:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    version = detect_version(repo_root, args.version)

    if args.command == "stage":
        destination = stage_payload(
            repo_root, args.board, args.product, args.target, args.output_dir.resolve()
        )
        print(destination)
    elif args.command == "install":
        destination = install_payload(
            repo_root,
            args.board,
            args.product,
            args.target,
            args.install_root.resolve(),
            version,
        )
        print(destination)
    elif args.command == "zip":
        print(
            create_zip(
                repo_root,
                args.board,
                args.product,
                args.target,
                args.output.resolve(),
                version,
            )
        )
    elif args.command == "targz":
        print(
            create_targz(
                repo_root,
                args.board,
                args.product,
                args.target,
                args.output.resolve(),
                version,
            )
        )


if __name__ == "__main__":
    main()
