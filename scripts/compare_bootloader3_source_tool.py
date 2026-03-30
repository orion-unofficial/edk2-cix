#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import pathlib
import platform
import shutil
import subprocess
import sys
import tempfile

from vendor_tool_resolver import resolve_vendor_tool


SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
CERT_COMPARE_PATH = SCRIPT_DIR / "compare_cix_regen_trusted_key_cert.py"


def load_cert_compare_module():
    spec = importlib.util.spec_from_file_location("cert_compare", CERT_COMPARE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load helper module: {CERT_COMPARE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


CERT_COMPARE = load_cert_compare_module()
VENDOR_TOOL_REPO_RELPATH = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/X86_64/cix_regen_trusted_key_cert"
DEFAULT_SOURCE_TOOL_DIR = REPO_ROOT / "src/tools/cix_regen_trusted_key_cert"
DEFAULT_SOURCE_TOOL = DEFAULT_SOURCE_TOOL_DIR / "cix_regen_trusted_key_cert"
DEFAULT_FIPTOOL = REPO_ROOT / "src/tools/arm-trusted-firmware-fiptool/build" / (
    "aarch64" if platform.machine() in {"aarch64", "arm64"} else "x86_64"
) / "fiptool"
DEFAULT_PUBLIC_KEY = REPO_ROOT / "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Keys/oem_publickey.pem"
DEFAULT_PRIVATE_KEY = REPO_ROOT / "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Keys/oem_privatekey.pem"


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(cmd: list[str], *, cwd: pathlib.Path | None = None) -> None:
    result = subprocess.run(
        cmd,
        cwd=cwd,
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(str(x) for x in cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


def create_bootloader3(
    fiptool: pathlib.Path,
    trusted_cert: pathlib.Path,
    nt_fw_key_cert: pathlib.Path,
    nt_fw_cert: pathlib.Path,
    nt_fw: pathlib.Path,
    output_path: pathlib.Path,
) -> None:
    run(
        [
            str(fiptool),
            "create",
            "--trusted-key-cert",
            str(trusted_cert),
            "--nt-fw-key-cert",
            str(nt_fw_key_cert),
            "--nt-fw-cert",
            str(nt_fw_cert),
            "--nt-fw",
            str(nt_fw),
            str(output_path),
        ]
    )


def unpack_bootloader3(fiptool: pathlib.Path, image: pathlib.Path, output_dir: pathlib.Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    local_image = output_dir / "bootloader3.img"
    shutil.copy2(image, local_image)
    run([str(fiptool), "unpack", local_image.name], cwd=output_dir)


def require_payloads(base_dir: pathlib.Path) -> dict[str, pathlib.Path]:
    required = {
        "nt-fw.bin": base_dir / "nt-fw.bin",
        "nt-fw-cert.bin": base_dir / "nt-fw-cert.bin",
        "nt-fw-key-cert.bin": base_dir / "nt-fw-key-cert.bin",
    }
    for path in required.values():
        if not path.is_file():
            raise FileNotFoundError(path)
    return required


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate bootloader3.img generation when cix_regen_trusted_key_cert is "
            "replaced by the source implementation."
        )
    )
    parser.add_argument(
        "input_dir",
        type=pathlib.Path,
        help="Directory containing nt-fw.bin, nt-fw-cert.bin, and nt-fw-key-cert.bin",
    )
    parser.add_argument(
        "--vendor-tool",
        type=pathlib.Path,
        help="Optional path to an external vendor cix_regen_trusted_key_cert binary",
    )
    parser.add_argument(
        "--vendor-ref",
        action="append",
        default=[],
        help=(
            "Git ref to search for the bundled vendor binary when --vendor-tool "
            "is omitted; may be repeated"
        ),
    )
    parser.add_argument("--source-tool", type=pathlib.Path, default=DEFAULT_SOURCE_TOOL)
    parser.add_argument("--source-tool-dir", type=pathlib.Path, default=DEFAULT_SOURCE_TOOL_DIR)
    parser.add_argument("--fiptool", type=pathlib.Path, default=DEFAULT_FIPTOOL)
    parser.add_argument("--public-key", type=pathlib.Path, default=DEFAULT_PUBLIC_KEY)
    parser.add_argument("--private-key", type=pathlib.Path, default=DEFAULT_PRIVATE_KEY)
    parser.add_argument("--skip-build", action="store_true", help="Do not rebuild the source tool")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    payloads = require_payloads(args.input_dir)

    required_paths = [
        args.input_dir,
        args.public_key,
        args.private_key,
    ]
    if not args.skip_build:
        if not args.source_tool_dir.is_dir():
            raise FileNotFoundError(args.source_tool_dir)
        CERT_COMPARE.run(["make", "-C", str(args.source_tool_dir), "clean"])
        CERT_COMPARE.run(["make", "-C", str(args.source_tool_dir), "all"])
        if not args.fiptool.exists():
            CERT_COMPARE.run(["make", "-C", str(REPO_ROOT / "src"), "host-fiptool"])
    required_paths.append(args.source_tool)
    required_paths.append(args.fiptool)

    for path in required_paths:
        if not path.exists():
            raise FileNotFoundError(path)

    refs = tuple(args.vendor_ref) if args.vendor_ref else None
    with resolve_vendor_tool(
        explicit_path=args.vendor_tool,
        repo_relpath=VENDOR_TOOL_REPO_RELPATH,
        refs=refs or ("main-monorepo-upstream", "main"),
    ) as vendor_tool:
        with tempfile.TemporaryDirectory(prefix="bootloader3-compare-") as td:
            td_path = pathlib.Path(td)
            vendor_trusted = td_path / "vendor-trusted.crt"
            source_trusted = td_path / "source-trusted.crt"
            vendor_bootloader = td_path / "vendor-bootloader3.img"
            source_bootloader = td_path / "source-bootloader3.img"
            vendor_unpack = td_path / "vendor"
            source_unpack = td_path / "source"

            CERT_COMPARE.run(
                [str(vendor_tool), "-p", str(args.public_key), "-s", str(args.private_key), "-o", str(vendor_trusted)]
            )
            CERT_COMPARE.run(
                [str(args.source_tool), "-p", str(args.public_key), "-s", str(args.private_key), "-o", str(source_trusted)]
            )
            CERT_COMPARE.compare_generate(vendor_trusted, source_trusted, args.public_key, args.private_key)

            create_bootloader3(
                args.fiptool,
                vendor_trusted,
                payloads["nt-fw-key-cert.bin"],
                payloads["nt-fw-cert.bin"],
                payloads["nt-fw.bin"],
                vendor_bootloader,
            )
            create_bootloader3(
                args.fiptool,
                source_trusted,
                payloads["nt-fw-key-cert.bin"],
                payloads["nt-fw-cert.bin"],
                payloads["nt-fw.bin"],
                source_bootloader,
            )
            unpack_bootloader3(args.fiptool, vendor_bootloader, vendor_unpack)
            unpack_bootloader3(args.fiptool, source_bootloader, source_unpack)

            for name in ("nt-fw.bin", "nt-fw-cert.bin", "nt-fw-key-cert.bin"):
                vendor_payload = vendor_unpack / name
                source_payload = source_unpack / name
                if vendor_payload.read_bytes() != payloads[name].read_bytes():
                    raise AssertionError(f"vendor bootloader3 changed payload {name}")
                if source_payload.read_bytes() != payloads[name].read_bytes():
                    raise AssertionError(f"source bootloader3 changed payload {name}")
                if vendor_payload.read_bytes() != source_payload.read_bytes():
                    raise AssertionError(f"vendor/source bootloader3 payload mismatch: {name}")

            vendor_trusted_unpacked = vendor_unpack / "trusted-key-cert.bin"
            source_trusted_unpacked = source_unpack / "trusted-key-cert.bin"
            if vendor_trusted_unpacked.read_bytes() != vendor_trusted.read_bytes():
                raise AssertionError("vendor bootloader3 did not embed the generated trusted-key cert")
            if source_trusted_unpacked.read_bytes() != source_trusted.read_bytes():
                raise AssertionError("source bootloader3 did not embed the generated trusted-key cert")

            CERT_COMPARE.compare_extract(args.source_tool, vendor_trusted_unpacked)
            if (
                source_bootloader.stat().st_size - vendor_bootloader.stat().st_size
                != source_trusted.stat().st_size - vendor_trusted.stat().st_size
            ):
                raise AssertionError("bootloader3 size delta does not match trusted-key-cert size delta")

            print("bootloader3 comparison passed")
            print(f"vendor trusted-key cert : {vendor_trusted.stat().st_size} bytes sha256={sha256(vendor_trusted)}")
            print(f"source trusted-key cert : {source_trusted.stat().st_size} bytes sha256={sha256(source_trusted)}")
            print(f"vendor bootloader3.img  : {vendor_bootloader.stat().st_size} bytes sha256={sha256(vendor_bootloader)}")
            print(f"source bootloader3.img  : {source_bootloader.stat().st_size} bytes sha256={sha256(source_bootloader)}")
            print("shared payloads         : nt-fw.bin, nt-fw-cert.bin, nt-fw-key-cert.bin are byte-identical")
            return 0


if __name__ == "__main__":
    sys.exit(main())
