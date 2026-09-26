#!/usr/bin/env python3

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

import replay_o6_release as replay


PACKAGE_ARTEFACTS = {
    "BuildOptions": "BuildOptions",
    "cix_flash_all.bin": "cix_flash_all.bin",
    "cix_flash_ota.bin": "cix_flash_ota.bin",
    "AARCH64/BurnImage.efi": "BurnImage.efi",
    "AARCH64/EnrollFromDefaultKeysApp.efi": "EnrollFromDefaultKeysApp.efi",
    "AARCH64/FlashUpdate.efi": "FlashUpdate.efi",
    "AARCH64/Shell.efi": "Shell.efi",
    "AARCH64/VariableInfo.efi": "VariableInfo.efi",
}
PACKAGE_REPO_PATHS = {
    "AARCH64/BurnImage.efi": (
        "src/edk2-non-osi/Platform/CIX/Sky1/FlashTool/BurnImage.efi"
    ),
    "AARCH64/FlashUpdate.efi": (
        "src/edk2-non-osi/Platform/CIX/Sky1/FlashTool/FlashUpdate.efi"
    ),
}


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_build_options(path: pathlib.Path) -> dict[str, object]:
    parsed: dict[str, object] = {"gCommandLineDefines": {}}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("gCommandLineDefines: "):
            parsed["gCommandLineDefines"] = ast.literal_eval(line.partition(": ")[2])
        elif line.startswith("Active Platform: "):
            parsed["active_platform_suffix"] = "src/" + line.split("/src/", 1)[-1]
        elif line.startswith("Flash Image Definition: "):
            parsed["flash_definition_suffix"] = "src/" + line.split("/src/", 1)[-1]
    return parsed


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Record portable replay timestamps, certificate payloads, and exact "
            "validation hashes from an already extracted Radxa release."
        )
    )
    parser.add_argument("--version", required=True)
    parser.add_argument("--package", type=pathlib.Path, required=True)
    parser.add_argument("--extracted-root", type=pathlib.Path, required=True)
    parser.add_argument("--output-root", type=pathlib.Path, required=True)
    parser.add_argument("--profile-file", type=pathlib.Path, required=True)
    parser.add_argument("--package-url")
    args = parser.parse_args()

    package = args.package.resolve()
    extracted_root = args.extracted_root.resolve()
    output_root = args.output_root.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    profile_file = args.profile_file.resolve()
    profile_data = json.loads(profile_file.read_text(encoding="utf-8"))
    profile_name = f"upstream-{args.version}-bookworm"
    previous_profile = profile_data.get("profiles", {}).get(profile_name, {})
    profile = {
        "description": (
            f"Exact published Radxa {args.version} replay on edk2-stable202208 "
            "with the recorded Bookworm inputs and certificate payloads."
        ),
        "boards": {},
    }
    release_manifest = {
        "version": args.version,
        "profile": profile_name,
        "package": {
            "url": args.package_url or (
                "https://github.com/radxa-pkg/edk2-cix/releases/download/"
                f"{args.version}/edk2-cix_{args.version}_all.deb"
            ),
            "sha256": sha256(package),
            "size": package.stat().st_size,
        },
        "boards": {},
    }

    with tempfile.TemporaryDirectory(prefix=".record-release-", dir=output_root) as tempdir:
        for board, board_config in replay.BOARD_CONFIG.items():
            extracted = extracted_root / args.version / board
            summary = json.loads((extracted / "replay-summary.json").read_text(encoding="utf-8"))
            release_dir = replay.extract_release_dir_from_deb(
                package,
                pathlib.Path(tempdir) / board,
                board_config["product"],
            )
            references = {
                label: release_dir / package_name
                for label, package_name in PACKAGE_ARTEFACTS.items()
            }
            pm_config = extracted / "reference" / "Firmwares" / "csu_pm_config.bin"
            if not pm_config.is_file():
                flash_config = replay.REPO_ROOT / "validation" / "replay-source" / args.version / replay.FLASH_CONFIG_ALL.relative_to(replay.REPO_ROOT)
                if not flash_config.is_file():
                    raise FileNotFoundError(
                        f"Missing recorded {args.version} flash layout: {flash_config}"
                    )
                package_extract = pathlib.Path(tempdir) / board / "package"
                package_extract.mkdir()
                subprocess.run(
                    [
                        sys.executable,
                        str(replay.PACKAGE_TOOL_SOURCE),
                        "-d",
                        str(release_dir / "cix_flash_all.bin"),
                        "-c",
                        str(flash_config),
                    ],
                    cwd=package_extract,
                    check=True,
                    stdout=subprocess.DEVNULL,
                )
                pm_config = package_extract / "unpack" / "csu_pm_config.bin"
            references["Firmwares/csu_pm_config.bin"] = pm_config
            references["FV/SKY1_BL33_UEFI.fd"] = extracted / "firmware" / "nt-fw.bin"
            missing = [str(path) for path in references.values() if not path.is_file()]
            if missing:
                raise FileNotFoundError("Missing replay references: " + ", ".join(missing))

            build_options = parse_build_options(references["BuildOptions"])
            defines = build_options["gCommandLineDefines"]
            if not isinstance(defines, dict) or defines.get("DEB_VERSION") != args.version:
                raise ValueError(
                    f"{board} BuildOptions does not describe release {args.version}: {defines}"
                )

            board_output = output_root / args.version / board
            cert_output = board_output / "certs"
            cert_output.mkdir(parents=True, exist_ok=True)
            for cert in sorted((extracted / "certs").iterdir()):
                if cert.is_file():
                    shutil.copy2(cert, cert_output / cert.name)

            env_values = {
                "BUILD_DATE": summary["build_date"],
                "SOURCE_DATE_EPOCH": str(summary["source_date_epoch"]),
                "PM_CONFIG_SOURCE_DATE_EPOCH": str(summary["pm_config_source_date_epoch"]),
                "SOURCE_COMMIT_HASH": summary["build_defines"]["COMMIT_HASH"],
                "EDK2_COMMIT_HASH": summary["build_defines"]["EDK2_COMMIT_HASH"],
                "EDK2_NON_OSI_COMMIT_HASH": summary["build_defines"]["EDK2_NON_OSI_COMMIT_HASH"],
                "EDK2_PLATFORMS_COMMIT_HASH": summary["build_defines"]["EDK2_PLATFORMS_COMMIT_HASH"],
            }
            (board_output / "replay.env").write_text(
                "\n".join(f"{key}={value}" for key, value in env_values.items()) + "\n",
                encoding="utf-8",
            )

            artefacts = {}
            for label, path in references.items():
                artefacts[label] = {
                    "path": label,
                    "sha256": sha256(path),
                    "size": path.stat().st_size,
                }
                if label in PACKAGE_REPO_PATHS:
                    artefacts[label]["repo_path"] = PACKAGE_REPO_PATHS[label]
            certs = {
                cert.name: {"sha256": sha256(cert), "size": cert.stat().st_size}
                for cert in sorted(cert_output.iterdir())
                if cert.is_file()
            }
            board_manifest = {
                "board": board,
                "product": board_config["product"],
                "replay_environment": env_values,
                "certificates": certs,
                "reference_artefacts": artefacts,
            }
            (board_output / "manifest.json").write_text(
                json.dumps(board_manifest, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            release_manifest["boards"][board] = {
                "manifest": f"{args.version}/{board}/manifest.json",
                "certificates": certs,
            }
            profile["boards"][board] = {
                "target": "RELEASE_GCC5",
                "artefacts": artefacts,
                "build_options": build_options,
                "pe_sections": previous_profile.get("boards", {})
                .get(board, {})
                .get("pe_sections", {}),
            }

    index_path = output_root / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.is_file() else {"releases": {}}
    index["releases"][args.version] = release_manifest
    index_path.write_text(json.dumps(index, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    profile_data.setdefault("profiles", {})[profile_name] = profile
    profile_file.write_text(
        json.dumps(profile_data, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"Recorded {args.version} replay inputs for O6 and O6N in {output_root}")
    print(f"Updated validation profile {profile_name} in {profile_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
