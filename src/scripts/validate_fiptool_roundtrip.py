#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import pathlib
import shutil
import subprocess
import tarfile
import tempfile


SCRIPT_PATH = pathlib.Path(__file__).resolve()
SRC_DIR = SCRIPT_PATH.parent.parent
REPO_ROOT = SRC_DIR.parent
PACKAGE_TOOL_DIR = SRC_DIR / "edk2-non-osi" / "Platform" / "CIX" / "Sky1" / "PackageTool"
PACKAGE_TOOL_SOURCE = PACKAGE_TOOL_DIR / "source_tools" / "cix_package_tool" / "cix_package_tool.py"
FIPTOOL_SOURCE_DIR = SRC_DIR / "tools" / "arm-trusted-firmware-fiptool"
FLASH_CONFIG_ALL = PACKAGE_TOOL_DIR / "spi_flash_config_all.json"
DEFAULT_TMP_ROOT = pathlib.Path(
    os.environ.get("EDK2_CIX_HOST_TMPDIR", tempfile.gettempdir())
).resolve()
DEFAULT_CONTAINER_TMPDIR = pathlib.PurePosixPath(
    os.environ.get("EDK2_CIX_CONTAINER_TMPDIR", "/hosttmp")
)
DEFAULT_CONTAINER_IMAGE = "mcr.microsoft.com/devcontainers/base:bookworm"
CONTAINER_RUNTIME_ENV = "EDK2_CIX_CONTAINER_RUNTIME"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate the vendored TF-A fiptool by unpacking and recreating an "
            "O6 bootloader3 image, then comparing the rebuilt FIP byte-for-byte."
        )
    )
    parser.add_argument(
        "input_path",
        help=(
            "Path to a published edk2-cix *.deb, a cix_flash_all.bin, a "
            "bootloader3.img, or an extracted O6 directory"
        ),
    )
    parser.add_argument(
        "--output-dir",
        help="Directory to write the validation report into",
    )
    parser.add_argument(
        "--keep-workdir",
        action="store_true",
        help="Keep the internal working directory for inspection",
    )
    parser.add_argument(
        "--container-image",
        default=DEFAULT_CONTAINER_IMAGE,
        help="Linux container image to use for the round-trip validation",
    )
    return parser.parse_args()


def resolve_container_runtime() -> str:
    override = os.environ.get(CONTAINER_RUNTIME_ENV)
    if override:
        return override

    for candidate in ("podman", "docker"):
        resolved = shutil.which(candidate)
        if not resolved:
            continue
        try:
            subprocess.run(
                [resolved, "info"],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            return resolved
        except (OSError, subprocess.CalledProcessError):
            continue

    for candidate in ("docker", "podman"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved

    raise RuntimeError("Neither podman nor docker is available on PATH")


def require_file(path: pathlib.Path) -> pathlib.Path:
    if not path.is_file():
        raise FileNotFoundError(path)
    return path


def ensure_dir(path: pathlib.Path) -> pathlib.Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_ar_members(archive_path: pathlib.Path) -> dict[str, bytes]:
    data = require_file(archive_path).read_bytes()
    if not data.startswith(b"!<arch>\n"):
        raise ValueError(f"Not an ar archive: {archive_path}")
    offset = 8
    members: dict[str, bytes] = {}
    while offset < len(data):
        header = data[offset : offset + 60]
        if len(header) < 60:
            break
        offset += 60
        name = header[0:16].decode("utf-8", errors="replace").strip().rstrip("/")
        size = int(header[48:58].decode("utf-8", errors="replace").strip())
        payload = data[offset : offset + size]
        offset += size
        if offset % 2:
            offset += 1
        members[name] = payload
    return members


def extract_release_dir_from_deb(deb_path: pathlib.Path, work_dir: pathlib.Path) -> pathlib.Path:
    release_root = ensure_dir(work_dir / "release")
    members = read_ar_members(deb_path)
    data_name, data_payload = next(
        ((name, payload) for name, payload in members.items() if name.startswith("data.tar.")),
        (None, None),
    )
    if data_name is None or data_payload is None:
        raise ValueError(f"Could not find data.tar.* inside {deb_path}")
    with tarfile.open(fileobj=io.BytesIO(data_payload), mode="r:*") as archive:
        archive.extractall(release_root)
    o6_dir = release_root / "usr" / "share" / "edk2" / "radxa" / "orion-o6"
    if not o6_dir.is_dir():
        raise ValueError(f"Could not find O6 payload directory inside {deb_path}")
    return o6_dir


def to_container_tmp_path(
    path: pathlib.Path,
    host_mount_root: pathlib.Path,
    container_mount_root: pathlib.PurePosixPath = DEFAULT_CONTAINER_TMPDIR,
) -> str:
    resolved = path.resolve()
    host_root = host_mount_root.resolve()
    relative = resolved.relative_to(host_root)
    return str(container_mount_root / pathlib.PurePosixPath(relative.as_posix()))


def stage_input(input_path: pathlib.Path, work_dir: pathlib.Path) -> tuple[str, pathlib.Path]:
    if input_path.is_dir():
        flash = input_path / "cix_flash_all.bin"
        if flash.is_file():
            staged = work_dir / "input" / "cix_flash_all.bin"
            ensure_dir(staged.parent)
            shutil.copy2(flash, staged)
            return "flash", staged
        bootloader = input_path / "bootloader3.img"
        if bootloader.is_file():
            staged = work_dir / "input" / "bootloader3.img"
            ensure_dir(staged.parent)
            shutil.copy2(bootloader, staged)
            return "bootloader", staged
        raise ValueError(f"Unsupported directory input: {input_path}")

    suffixes = input_path.suffixes
    if suffixes[-1:] == [".deb"]:
        release_dir = extract_release_dir_from_deb(input_path, work_dir)
        staged = work_dir / "input" / "cix_flash_all.bin"
        ensure_dir(staged.parent)
        shutil.copy2(require_file(release_dir / "cix_flash_all.bin"), staged)
        return "flash", staged

    if input_path.name == "bootloader3.img" or input_path.suffix == ".img":
        staged = work_dir / "input" / "bootloader3.img"
        ensure_dir(staged.parent)
        shutil.copy2(require_file(input_path), staged)
        return "bootloader", staged

    if input_path.suffix == ".bin":
        staged = work_dir / "input" / "cix_flash_all.bin"
        ensure_dir(staged.parent)
        shutil.copy2(require_file(input_path), staged)
        return "flash", staged

    raise ValueError(f"Unsupported input type: {input_path}")


def main() -> int:
    args = parse_args()
    input_path = pathlib.Path(args.input_path).resolve()
    output_dir = (
        pathlib.Path(args.output_dir).resolve()
        if args.output_dir
        else pathlib.Path(tempfile.mkdtemp(prefix="fiptool-roundtrip-", dir=str(DEFAULT_TMP_ROOT)))
    )
    ensure_dir(output_dir)

    work_obj: tempfile.TemporaryDirectory[str] | None = None
    if args.keep_workdir:
        work_dir = ensure_dir(output_dir / "work")
    else:
        work_obj = tempfile.TemporaryDirectory(prefix="fiptool-roundtrip-work-", dir=str(DEFAULT_TMP_ROOT))
        work_dir = pathlib.Path(work_obj.name)

    input_kind, staged_input = stage_input(input_path, work_dir)
    host_tmp_root = work_dir.parent.resolve()
    container_tmp_root = DEFAULT_CONTAINER_TMPDIR
    roundtrip_dir = ensure_dir(work_dir / "roundtrip")
    report_path = output_dir / "report.json"

    container_command = f"""
set -euo pipefail
pkg={('/workspace/' + str(PACKAGE_TOOL_DIR.relative_to(REPO_ROOT)))}
pkg_tool={('/workspace/' + str(PACKAGE_TOOL_SOURCE.relative_to(REPO_ROOT)))}
fiptool_src={('/workspace/' + str(FIPTOOL_SOURCE_DIR.relative_to(REPO_ROOT)))}
work={to_container_tmp_path(work_dir, host_tmp_root, container_tmp_root)}
mkdir -p "$work/roundtrip"
make -C "$fiptool_src" HOST_ARCH=x86_64 >/dev/null
fiptool="$fiptool_src/build/x86_64/fiptool"
cd "$work/roundtrip"
"""
    if input_kind == "flash":
        container_command += f"""
python3 "$pkg_tool" -d "{to_container_tmp_path(staged_input, host_tmp_root, container_tmp_root)}" -c "$pkg/spi_flash_config_all.json" >/dev/null
cp unpack/bootloader3.img original.bin
"""
    else:
        container_command += f"""
cp "{to_container_tmp_path(staged_input, host_tmp_root, container_tmp_root)}" original.bin
"""
    container_command += f"""
"$fiptool" unpack original.bin >/dev/null
"$fiptool" create \
  --trusted-key-cert trusted-key-cert.bin \
  --nt-fw-key-cert nt-fw-key-cert.bin \
  --nt-fw-cert nt-fw-cert.bin \
  --nt-fw nt-fw.bin \
  recreated.bin >/dev/null
"""

    container_runtime = resolve_container_runtime()
    subprocess.run(
        [
            container_runtime,
            "run",
            "--rm",
            "--platform",
            "linux/amd64",
            "-v",
            f"{REPO_ROOT}:/workspace",
            "-v",
            f"{host_tmp_root}:{container_tmp_root}",
            "-w",
            str(container_tmp_root),
            args.container_image,
            "bash",
            "-lc",
            container_command,
        ],
        check=True,
        text=True,
    )

    original = require_file(roundtrip_dir / "original.bin")
    recreated = require_file(roundtrip_dir / "recreated.bin")
    report = {
        "identical": original.read_bytes() == recreated.read_bytes(),
        "original_sha256": sha256(original),
        "recreated_sha256": sha256(recreated),
        "original_size": original.stat().st_size,
        "recreated_size": recreated.stat().st_size,
    }
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    if work_obj is not None:
        work_obj.cleanup()
    return 0 if report.get("identical") else 1


if __name__ == "__main__":
    raise SystemExit(main())
