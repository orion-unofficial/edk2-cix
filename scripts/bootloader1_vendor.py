#!/usr/bin/env python3
"""Run CIX BL1 signature verification, distinguishing rejection from unavailability."""

from __future__ import annotations

import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

from reconstruction_common import ReconstructionError, temp_dir


ROOT = Path(__file__).resolve().parents[1]
CONTAINER_IMAGE = "debian:trixie-slim@sha256:d7e12182ce18b85b93007c1dedf31f2d29e01ccf3182cc4017c709b6259bc132"


class VendorUnavailable(RuntimeError):
    """The verifier could not be obtained or started."""


def prepare_tool(tool: Path | None = None, *, download: bool = True) -> Path:
    metadata = json.loads((ROOT / "config/bootloader1-payloads.json").read_text())["vendor_tool"]
    override = os.environ.get("EDK2_CIX_BL1_TOOL")
    if tool is None and override:
        tool = Path(override).expanduser()
        download = False
    if tool is None:
        tool = ROOT / ".cache/edk2-cix/bl1-vendor" / metadata["sha256"] / "cix_mkimage_rsa"
    try:
        if not tool.exists() and download:
            print("[bl1] Downloading the pinned CIX signature verifier", file=sys.stderr, flush=True)
            with urllib.request.urlopen(metadata["url"], timeout=30) as response:
                data = response.read(16 * 1024 * 1024)
            if hashlib.sha256(data).hexdigest() != metadata["sha256"]:
                raise VendorUnavailable("downloaded vendor verifier checksum mismatch; refusing to execute it")
            tool.parent.mkdir(parents=True, exist_ok=True)
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=tool.parent, prefix=".download-", delete=False) as stream:
                    temporary = Path(stream.name)
                    stream.write(data)
                temporary.chmod(0o755)
                temporary.replace(tool)
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
        if not tool.is_file() or hashlib.sha256(tool.read_bytes()).hexdigest() != metadata["sha256"]:
            raise VendorUnavailable("vendor verifier is missing or does not match the pinned checksum")
    except (OSError, TimeoutError) as exc:
        raise VendorUnavailable(f"cannot obtain vendor verifier: {exc}") from exc
    return tool.resolve()


def select_runner(requested: str = "auto") -> str:
    native = platform.system() == "Linux" and platform.machine() in {"x86_64", "AMD64"}
    if requested == "native":
        if not native:
            raise VendorUnavailable("native CIX verifier requires x86-64 Linux")
        return "native"
    if requested == "auto" and native:
        return "native"
    for runtime in (("docker", "podman") if requested == "auto" else (requested,)):
        if runtime in {"docker", "podman"} and shutil.which(runtime):
            return runtime
    raise VendorUnavailable("no Docker or Podman runtime is available for the x86-64 Linux verifier")


def command(tool: Path, directory: Path, runner: str, arguments: list[str]) -> list[str]:
    if runner == "native":
        return [str(tool), *arguments]
    return [runner, "run", "--rm", "--platform", "linux/amd64", "--network", "none",
            "--read-only", "--cap-drop", "ALL", "--security-opt", "no-new-privileges",
            "--pids-limit", "32", "--memory", "256m",
            "--mount", f"type=bind,src={tool},dst=/vendor/cix_mkimage_rsa,readonly",
            "--mount", f"type=bind,src={ROOT / 'config'},dst=/config,readonly",
            "--mount", f"type=bind,src={directory},dst=/images,readonly", "--workdir", "/config",
            CONTAINER_IMAGE, "/vendor/cix_mkimage_rsa", *arguments]


def execute(cmd: list[str], *, probe: bool, container: bool) -> subprocess.CompletedProcess:
    try:
        result = subprocess.run(cmd, cwd=ROOT / "config", capture_output=True, text=True, timeout=60)
    except OSError as exc:
        raise VendorUnavailable(f"cannot launch vendor verifier: {exc}") from exc
    except subprocess.TimeoutExpired as exc:
        if probe:
            raise VendorUnavailable("vendor verifier launch probe timed out") from exc
        raise ReconstructionError("BL1 vendor verification timed out after a successful launch probe") from exc
    # These container-runtime statuses mean the image command was not launched.
    if container and result.returncode in {125, 126, 127}:
        raise VendorUnavailable(f"container could not run vendor verifier: {result.stderr.strip()}")
    if probe and (result.returncode or "--verify" not in result.stdout):
        raise VendorUnavailable(f"vendor verifier launch probe failed: {(result.stdout + result.stderr).strip()}")
    return result


def verify_payloads(payloads: dict[str, bytes], *, tool: Path | None = None,
                    runner: str = "auto", download: bool = True) -> dict:
    selected = select_runner(runner)
    executable = prepare_tool(tool, download=download)
    print(f"[bl1] Checking vendor signatures using {selected}", file=sys.stderr, flush=True)
    records = []
    with temp_dir(ROOT, "bl1-vendor-") as temporary:
        directory = Path(temporary).resolve()
        execute(command(executable, directory, selected, ["--help"]), probe=True, container=selected != "native")
        for digest, data in payloads.items():
            if hashlib.sha256(data).hexdigest() != digest:
                raise ReconstructionError("BL1 bytes changed before vendor verification")
            # Use immutable per-payload paths across container invocations.
            image = directory / f"{digest}.img"
            image.write_bytes(data)
            image_arg = str(image) if selected == "native" else f"/images/{image.name}"
            result = execute(command(executable, directory, selected,
                                     ["-j", "bootloader1-vendor-verify.json", "-v", image_arg]),
                             probe=False, container=selected != "native")
            log = result.stdout + result.stderr
            if result.returncode or "Verify signature OK" not in log or "Compare hash data of image" not in log:
                raise ReconstructionError(f"BL1 vendor signature verification FAILED ({digest}):\n{log}")
            records.append({"sha256": digest, "verifier_output": log})
    return {"status": "verified", "runner": selected, "payloads": records}


def verify_or_warn(payloads: dict[str, bytes]) -> dict:
    try:
        result = verify_payloads(payloads)
        print(f"[bl1] Vendor signatures verified for {len(payloads)} payload(s)")
        return result
    except VendorUnavailable as exc:
        print(f"[bl1] WARNING: vendor signature verification unavailable: {exc}. "
              "Only exact copies of approved upstream vendor BL1 payloads are accepted by the "
              "mandatory SHA-256 and size check, including CIX 2026Q1.", file=sys.stderr)
        return {"status": "unavailable", "warning": str(exc)}
