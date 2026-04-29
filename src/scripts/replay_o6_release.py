#!/usr/bin/env python3

from __future__ import annotations

import argparse
import ast
import datetime as dt
import hashlib
import io
import json
import pathlib
import re
import shlex
import shutil
import subprocess
import tarfile
import tempfile
from typing import Iterable


SCRIPT_PATH = pathlib.Path(__file__).resolve()
SRC_DIR = SCRIPT_PATH.parent.parent
REPO_ROOT = SRC_DIR.parent
PACKAGE_TOOL_DIR = SRC_DIR / "edk2-non-osi" / "Platform" / "CIX" / "Sky1" / "PackageTool"
PM_CONFIG_DIR = SRC_DIR / "edk2-platforms" / "Platform" / "Radxa" / "Orion" / "O6" / "pm_config"
FLASH_CONFIG_ALL = PACKAGE_TOOL_DIR / "spi_flash_config_all.json"
PACKAGE_TOOL_BIN = PACKAGE_TOOL_DIR / "X86_64" / "cix_package_tool"
FIPTOOL_BIN = PACKAGE_TOOL_DIR / "X86_64" / "fiptool"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract replay inputs from an O6 release artefact and optionally "
            "rebuild matching firmware on main-monorepo."
        )
    )
    parser.add_argument(
        "input_path",
        help="Path to a published edk2-cix *.deb, a cix_flash_all.bin, or an extracted O6 directory",
    )
    parser.add_argument(
        "--build-options",
        help="Optional BuildOptions file to pair with a .bin input when the ISO BUILD_DATE is not otherwise available",
    )
    parser.add_argument(
        "--build-date",
        help="Optional BUILD_DATE override for .bin input when no BuildOptions file is available",
    )
    parser.add_argument(
        "--output-dir",
        help="Directory to write replay.env, certs/, and helper scripts into (defaults to a fresh /private/tmp directory)",
    )
    parser.add_argument(
        "--run-build",
        action="store_true",
        help="Run the generated replay build now using the current environment",
    )
    parser.add_argument(
        "--compare",
        action="store_true",
        help="Compare rebuilt outputs against the input artefact after a successful build",
    )
    parser.add_argument(
        "--keep-workdir",
        action="store_true",
        help="Keep the internal temporary extraction work directory for inspection",
    )
    return parser.parse_args()


def require_file(path: pathlib.Path) -> pathlib.Path:
    if not path.is_file():
        raise FileNotFoundError(path)
    return path


def ensure_clean_dir(path: pathlib.Path) -> pathlib.Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def iso_to_epoch(text: str) -> int:
    return int(dt.datetime.fromisoformat(text).timestamp())


def to_hosttmp_path(path: pathlib.Path) -> str:
    path_str = str(path.resolve())
    for prefix in ("/private/tmp/", "/tmp/"):
        if path_str.startswith(prefix):
            return f"/hosttmp/{path_str[len(prefix):]}"
    raise ValueError(f"Path is not visible inside the helper container: {path}")


def run(argv: list[str], **kwargs) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, check=True, text=True, capture_output=True, **kwargs)


def detect_input_kind(path: pathlib.Path) -> str:
    if path.is_dir():
        return "dir"
    suffixes = path.suffixes
    if suffixes[-1:] == [".deb"]:
        return "deb"
    if suffixes[-1:] == [".bin"]:
        return "bin"
    raise ValueError(f"Unsupported input type: {path}")


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
        name = header[0:16].decode("utf-8", errors="replace").strip()
        size_text = header[48:58].decode("utf-8", errors="replace").strip()
        try:
            size = int(size_text)
        except ValueError as exc:
            raise ValueError(f"Invalid ar member size in {archive_path}: {size_text!r}") from exc
        payload = data[offset : offset + size]
        offset += size
        if offset % 2:
            offset += 1
        members[name.rstrip("/")] = payload
    return members


def extract_release_dir_from_deb(deb_path: pathlib.Path, work_dir: pathlib.Path) -> pathlib.Path:
    release_root = ensure_clean_dir(work_dir / "release")
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


def parse_build_options(build_options: pathlib.Path) -> dict[str, str]:
    first_line = require_file(build_options).read_text(encoding="utf-8").splitlines()[0]
    match = re.search(r"gCommandLineDefines:\s*(\{.*\})", first_line)
    if not match:
        raise ValueError(f"Could not parse BUILD_DATE from {build_options}")
    return ast.literal_eval(match.group(1))


def parse_compile_epoch(flash_image: pathlib.Path) -> tuple[str, str, int]:
    output = run(["strings", "-a", str(require_file(flash_image))]).stdout.splitlines()
    for index, line in enumerate(output):
        if "built at %a on %a" not in line:
            continue
        if index < 2:
            continue
        build_date = output[index - 2].strip()
        build_time = output[index - 1].strip()
        build_dt = dt.datetime.strptime(
            f"{build_date} {build_time}", "%b %d %Y %H:%M:%S"
        ).replace(tzinfo=dt.timezone.utc)
        return build_date, build_time, int(build_dt.timestamp())
    raise ValueError(f"Could not locate compile timestamp banner inside {flash_image}")


def extract_flash_details(flash_image: pathlib.Path, work_dir: pathlib.Path) -> tuple[int, pathlib.Path]:
    input_dir = ensure_clean_dir(work_dir / "input")
    flash_dir = ensure_clean_dir(work_dir / "flash")
    pm_parse_path = work_dir / "pm_parse.txt"

    staged_flash = input_dir / "cix_flash_all.bin"
    shutil.copy2(flash_image, staged_flash)

    container_command = f"""
set -euo pipefail
pkg={shlex.quote('/workspace/' + str(PACKAGE_TOOL_DIR.relative_to(REPO_ROOT)))}
pm={shlex.quote('/workspace/' + str(PM_CONFIG_DIR.relative_to(REPO_ROOT)))}
work={shlex.quote(to_hosttmp_path(work_dir))}
mkdir -p "$work/flash"
cd "$work/flash"
"$pkg/X86_64/cix_package_tool" -d "$work/input/cix_flash_all.bin" -c "$pkg/spi_flash_config_all.json" >/dev/null
cp unpack/bootloader3.img .
"$pkg/X86_64/fiptool" unpack bootloader3.img >/dev/null
make -C "$pm" csupm_bin_config >/dev/null
"$pm/csupm_bin_config" unpack/csu_pm_config.bin > "$work/pm_parse.txt"
"""

    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--platform",
            "linux/amd64",
            "-v",
            f"{REPO_ROOT}:/workspace",
            "-v",
            "/private/tmp:/hosttmp",
            "-w",
            "/hosttmp",
            "mcr.microsoft.com/devcontainers/base:bookworm",
            "bash",
            "-lc",
            container_command,
        ],
        check=True,
        text=True,
    )

    pm_parse = require_file(pm_parse_path).read_text(encoding="utf-8")
    match = re.search(r"timestamp\s*:\s*.*-\s*(.+)$", pm_parse, re.MULTILINE)
    if not match:
        raise ValueError(f"Could not parse PM timestamp from {pm_parse_path}")
    human = match.group(1).strip()
    pm_dt = dt.datetime.strptime(human, "%a %b %d %H:%M:%S %Y").replace(
        tzinfo=dt.timezone.utc
    )
    return int(pm_dt.timestamp()), flash_dir


def copy_cert_bundle(flash_dir: pathlib.Path, output_dir: pathlib.Path) -> pathlib.Path:
    cert_dir = ensure_clean_dir(output_dir / "certs")
    for name in ("trusted-key-cert.bin", "nt-fw-cert.bin", "nt-fw-key-cert.bin"):
        shutil.copy2(require_file(flash_dir / name), cert_dir / name)
    return cert_dir


def write_env_file(env_path: pathlib.Path, env_values: dict[str, str]) -> None:
    lines = [f"{key}={value}" for key, value in env_values.items()]
    env_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_rebuild_wrapper(
    wrapper_path: pathlib.Path,
    env_values: dict[str, str],
    build_targets: Iterable[str],
) -> None:
    quoted_targets = " ".join(shlex.quote(target) for target in build_targets)
    make_vars = [
        f"BUILD_DATE={shlex.quote(env_values['BUILD_DATE'])}",
        "ARTEFACT_MODE=upstream",
        f"SOURCE_DATE_EPOCH={shlex.quote(env_values['SOURCE_DATE_EPOCH'])}",
        f"PM_CONFIG_SOURCE_DATE_EPOCH={shlex.quote(env_values['PM_CONFIG_SOURCE_DATE_EPOCH'])}",
        f"SIGNING_CERT_SOURCE_DIR={shlex.quote(env_values['SIGNING_CERT_SOURCE_DIR'])}",
    ]
    for key in (
        "SOURCE_COMMIT_HASH",
        "EDK2_COMMIT_HASH",
        "EDK2_NON_OSI_COMMIT_HASH",
        "EDK2_PLATFORMS_COMMIT_HASH",
    ):
        if key in env_values:
            make_vars.append(f"{key}={shlex.quote(env_values[key])}")
    wrapper = f"""#!/usr/bin/env bash
set -euo pipefail

cd {shlex.quote(str(REPO_ROOT))}
make -C src clean
make -C src {' '.join(make_vars)} {quoted_targets}
"""
    wrapper_path.write_text(wrapper, encoding="utf-8")
    wrapper_path.chmod(0o755)


def write_docker_rebuild_wrapper(
    wrapper_path: pathlib.Path,
    env_values: dict[str, str],
    build_targets: Iterable[str],
) -> None:
    cert_dir_hosttmp = to_hosttmp_path(pathlib.Path(env_values["SIGNING_CERT_SOURCE_DIR"]))
    quoted_targets = " ".join(shlex.quote(target) for target in build_targets)
    make_vars = [
        f"BUILD_DATE={shlex.quote(env_values['BUILD_DATE'])}",
        "ARTEFACT_MODE=upstream",
        f"SOURCE_DATE_EPOCH={shlex.quote(env_values['SOURCE_DATE_EPOCH'])}",
        f"PM_CONFIG_SOURCE_DATE_EPOCH={shlex.quote(env_values['PM_CONFIG_SOURCE_DATE_EPOCH'])}",
        f"SIGNING_CERT_SOURCE_DIR={shlex.quote(cert_dir_hosttmp)}",
    ]
    for key in (
        "SOURCE_COMMIT_HASH",
        "EDK2_COMMIT_HASH",
        "EDK2_NON_OSI_COMMIT_HASH",
        "EDK2_PLATFORMS_COMMIT_HASH",
    ):
        if key in env_values:
            make_vars.append(f"{key}={shlex.quote(env_values[key])}")
    wrapper = f"""#!/usr/bin/env bash
set -euo pipefail

cd {shlex.quote(str(REPO_ROOT))}
./scripts/run_in_buildbox.sh make -C src clean
./scripts/run_in_buildbox.sh make -C src {' '.join(make_vars)} {quoted_targets}
"""
    wrapper_path.write_text(wrapper, encoding="utf-8")
    wrapper_path.chmod(0o755)


def compare_outputs(reference_files: dict[str, pathlib.Path], board: str = "O6") -> list[str]:
    built_root = SRC_DIR / "Build" / board / "RELEASE_GCC5"
    built_files = {
        "cix_flash_all.bin": built_root / "cix_flash_all.bin",
        "cix_flash_ota.bin": built_root / "cix_flash_ota.bin",
        "BuildOptions": built_root / "BuildOptions",
    }
    results: list[str] = []
    for name, reference in reference_files.items():
        built = built_files.get(name)
        if built is None or not reference.is_file() or not built.is_file():
            continue
        reference_hash = sha256(reference)
        built_hash = sha256(built)
        status = "IDENTICAL" if reference_hash == built_hash else "DIFFER"
        results.append(f"{name}: {status} {reference_hash} {built_hash}")
    return results


def main() -> int:
    args = parse_args()
    input_path = pathlib.Path(args.input_path).resolve()
    input_kind = detect_input_kind(input_path)

    output_dir = (
        pathlib.Path(args.output_dir).resolve()
        if args.output_dir
        else pathlib.Path(tempfile.mkdtemp(prefix="o6-replay-", dir="/private/tmp"))
    )
    ensure_clean_dir(output_dir)

    work_dir_obj: tempfile.TemporaryDirectory[str] | None = None
    if args.keep_workdir:
        work_dir = ensure_clean_dir(output_dir / "work")
    else:
        work_dir_obj = tempfile.TemporaryDirectory(prefix="o6-replay-work-", dir="/private/tmp")
        work_dir = pathlib.Path(work_dir_obj.name)

    reference_files: dict[str, pathlib.Path] = {}
    build_defines: dict[str, str] = {}

    if input_kind == "deb":
        release_dir = extract_release_dir_from_deb(input_path, work_dir)
        reference_files["cix_flash_all.bin"] = require_file(release_dir / "cix_flash_all.bin")
        reference_files["cix_flash_ota.bin"] = require_file(release_dir / "cix_flash_ota.bin")
        reference_files["BuildOptions"] = require_file(release_dir / "BuildOptions")
        build_defines = parse_build_options(reference_files["BuildOptions"])
    elif input_kind == "dir":
        reference_files["cix_flash_all.bin"] = require_file(input_path / "cix_flash_all.bin")
        if (input_path / "cix_flash_ota.bin").is_file():
            reference_files["cix_flash_ota.bin"] = input_path / "cix_flash_ota.bin"
        if (input_path / "BuildOptions").is_file():
            reference_files["BuildOptions"] = input_path / "BuildOptions"
            build_defines = parse_build_options(reference_files["BuildOptions"])
    else:
        reference_files["cix_flash_all.bin"] = require_file(input_path)
        if args.build_options:
            reference_files["BuildOptions"] = pathlib.Path(args.build_options).resolve()
            build_defines = parse_build_options(reference_files["BuildOptions"])

    build_date = build_defines.get("BUILD_DATE")
    if build_date is None and args.build_date:
        build_date = args.build_date

    compile_date, compile_time, source_date_epoch = parse_compile_epoch(
        reference_files["cix_flash_all.bin"]
    )
    pm_config_source_date_epoch, flash_dir = extract_flash_details(
        reference_files["cix_flash_all.bin"], work_dir
    )
    cert_dir = copy_cert_bundle(flash_dir, output_dir)

    env_values = {
        "ARTEFACT_MODE": "upstream",
        "SOURCE_DATE_EPOCH": str(source_date_epoch),
        "PM_CONFIG_SOURCE_DATE_EPOCH": str(pm_config_source_date_epoch),
        "SIGNING_CERT_SOURCE_DIR": str(cert_dir),
    }
    if build_date is not None:
        env_values["BUILD_DATE"] = build_date
    for key in (
        "COMMIT_HASH",
        "EDK2_COMMIT_HASH",
        "EDK2_NON_OSI_COMMIT_HASH",
        "EDK2_PLATFORMS_COMMIT_HASH",
    ):
        value = build_defines.get(key)
        if not value:
            continue
        env_key = "SOURCE_COMMIT_HASH" if key == "COMMIT_HASH" else key
        env_values[env_key] = value

    env_path = output_dir / "replay.env"
    write_env_file(env_path, env_values)

    wrapper_path = output_dir / "rebuild-o6.sh"
    docker_wrapper_path = output_dir / "rebuild-o6-docker.sh"
    if "BUILD_DATE" in env_values:
        write_rebuild_wrapper(
            wrapper_path,
            env_values,
            (
                "Build/O6/RELEASE_GCC5/cix_flash_all.bin",
                "Build/O6/RELEASE_GCC5/cix_flash_ota.bin",
            ),
        )
        try:
            write_docker_rebuild_wrapper(
                docker_wrapper_path,
                env_values,
                (
                    "Build/O6/RELEASE_GCC5/cix_flash_all.bin",
                    "Build/O6/RELEASE_GCC5/cix_flash_ota.bin",
                ),
            )
        except ValueError:
            docker_wrapper_path = None

    summary = {
        "input_path": str(input_path),
        "output_dir": str(output_dir),
        "build_date": build_date,
        "source_commit": build_defines.get("COMMIT_HASH"),
        "compile_build_date": compile_date,
        "compile_build_time": compile_time,
        "source_date_epoch": source_date_epoch,
        "pm_config_source_date_epoch": pm_config_source_date_epoch,
        "signing_cert_source_dir": str(cert_dir),
        "build_defines": build_defines,
        "reference_files": {name: str(path) for name, path in reference_files.items()},
    }
    (output_dir / "replay-summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    print(f"Replay helper output: {output_dir}")
    if build_date is None:
        print("Missing BUILD_DATE: provide --build-options <BuildOptions> or --build-date <iso8601> for a full replay build.")
    print("Replay settings:")
    for key, value in env_values.items():
        print(f"  {key}={value}")
    print("Shell exports:")
    for key, value in env_values.items():
        print(f"export {key}={shlex.quote(value)}")
    if "BUILD_DATE" in env_values:
        print(f"Host rebuild wrapper: {wrapper_path}")
        if docker_wrapper_path is not None:
            print(f"Docker rebuild wrapper: {docker_wrapper_path}")
    if build_defines.get("COMMIT_HASH"):
        print(f"Upstream source commit: {build_defines['COMMIT_HASH']}")
    print(f"Compiler timestamp recovered from flash image: {compile_date} {compile_time}")

    if args.run_build:
        if "BUILD_DATE" not in env_values:
            raise SystemExit("Cannot run replay build without BUILD_DATE")
        subprocess.run([str(wrapper_path)], check=True)
        if args.compare:
            print("Comparison against reference files:")
            for line in compare_outputs(reference_files):
                print(f"  {line}")

    if work_dir_obj is not None:
        work_dir_obj.cleanup()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())