#!/usr/bin/env python3
"""Read-only structural preflight for retained custom Radxa 1.3 source trees.

This checks source semantics that can silently regress during a clean overlay
merge.  It is deliberately limited to the EDK2/Radxa layouts listed below;
it does not establish build or hardware correctness.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import posixpath
import re
import subprocess
from pathlib import Path


O6 = "Platform/Radxa/Orion/O6"
SMBIOS = f"edk2-platforms/{O6}/Drivers/PlatformSmbios"
O6_ACPI = f"custom/overlay/edk2-platforms/{O6}/Drivers/AcpiPlatfomTables"
CPU_ASL = "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Dsdt-CPU.asl"
PCIE_MENU = "edk2-platforms/Platform/CIX/Sky1/Drivers/SetupManagerDxe/PcieMenu"
SUPPORTED_PROFILES = {
    ("202208", "1.3.0"): ("overlay", 0x1F4000),
    ("202208", "1.3.1"): ("overlay", 0x1F4000),
    ("202211", "1.3.0"): ("overlay", 0x1F4000),
    ("202605", "1.3.0"): ("experimental", 0x1F2000),
    ("202605", "1.3.1"): ("experimental", 0x1F2000),
    ("202608", "1.3.0"): ("overlay", 0x1F4000),
    ("202608", "1.3.1"): ("overlay", 0x1F4000),
}


@dataclass(frozen=True)
class Entry:
    mode: str
    oid: str


class GitTree:
    """Inspect one commit/tree without checkout, ref changes or build output."""

    def __init__(self, repo: Path, revision: str):
        self.repo = repo
        self.revision = revision
        raw = self._git("ls-tree", "-rz", revision)
        self.entries: dict[str, Entry] = {}
        for item in raw.split(b"\0"):
            if item:
                metadata, path = item.split(b"\t", 1)
                mode, _kind, oid = metadata.decode("ascii").split()
                self.entries[path.decode("utf-8", "surrogateescape")] = Entry(mode, oid)
        if not self.entries:
            raise ValueError(f"empty or missing source tree: {revision}")

    def _git(self, *args: str) -> bytes:
        return subprocess.run(
            ["git", "-C", str(self.repo), *args],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        ).stdout

    def blob(self, path: str) -> bytes:
        return self._git("cat-file", "blob", self.entries[path].oid)

    def text(self, path: str) -> str:
        return self.blob(path).decode("utf-8", "replace")

    def resolve(self, path: str) -> str:
        """Follow tracked relative symlinks, rejecting escape and cycles."""
        seen = set()
        while path in self.entries and self.entries[path].mode == "120000":
            if path in seen:
                raise ValueError(f"symlink cycle at {path}")
            seen.add(path)
            target = self.text(path).strip()
            if posixpath.isabs(target):
                raise ValueError(f"absolute overlay symlink: {path}")
            path = posixpath.normpath(posixpath.join(posixpath.dirname(path), target))
            if path == ".." or path.startswith("../"):
                raise ValueError(f"overlay symlink leaves source tree: {path}")
        if path not in self.entries:
            raise ValueError(f"overlay symlink target is missing: {path}")
        return path


def _required(tree: GitTree, path: str, problems: list[str]) -> str:
    if path not in tree.entries:
        problems.append(f"missing {path}")
        return ""
    try:
        return tree.text(tree.resolve(path))
    except ValueError as exc:
        problems.append(str(exc))
        return ""


def _smbios(tree: GitTree, problems: list[str]) -> None:
    root = f"custom/overlay/{SMBIOS}"
    for name in ("PlatformSmbios.c", "PlatformSmbios.h", "PlatformSmbios.inf"):
        path = f"{root}/{name}"
        expected_mode = "100644" if name.endswith(".inf") else "120000"
        if tree.entries.get(path, Entry("", "")).mode != expected_mode:
            problems.append(f"{path}: expected Git mode {expected_mode}")
        data = _required(tree, path, problems)
        # The 1.3 vendor source removed the Type 4/7 providers.  A surviving
        # custom list/header/INF reference is a clean-merge compile failure.
        if re.search(r"(?:SmbiosType|SMBIOS_TYPE|Type)[47](?!\d)", data):
            problems.append(f"{path}: references removed SMBIOS Type 4/7 provider")
    for name in ("SmbiosType4.c", "SmbiosType7.c"):
        if f"{root}/{name}" in tree.entries:
            problems.append(f"{root}/{name}: obsolete 1.2 provider retained")


def _cppc(tree: GitTree, problems: list[str]) -> None:
    data = _required(tree, CPU_ASL, problems)
    # Match each Device body up to the next Device rather than relying on line
    # offsets, which differ across EDK2 vintages.
    devices = list(re.finditer(r"\bDevice\s*\(\s*CPU(\d+)\s*\)", data))
    bodies = {int(match.group(1)): data[match.end():devices[index + 1].start()
                                      if index + 1 < len(devices) else len(data)]
              for index, match in enumerate(devices)}
    for cpu in (0, 1, 4, 5):
        body = bodies.get(cpu, "")
        desired = "CORE_0_TO_3" if cpu < 4 else "CORE_4_5"
        arch = "CIX_A520_REF_PERF" if cpu < 4 else "CIX_A720_REF_PERF"
        calls = re.findall(r"CPPC_PACKAGE_INIT\s*\(([^)]*)\)", body)
        args = [part.strip() for part in calls[0].split(",")] if len(calls) == 1 else []
        if not args or args[0] != desired + "_DESIRED_PERF_REG":
            problems.append(f"{CPU_ASL}: CPU{cpu} has wrong CPPC register group")
        if not args or args[-1] != arch:
            problems.append(f"{CPU_ASL}: CPU{cpu} has wrong CPPC reference performance")


def _release_fdf(tree: GitTree, edk2: str, radxa: str, problems: list[str]) -> None:
    layout, release_size = SUPPORTED_PROFILES[(edk2, radxa)]
    ordinary = f"custom/overlay/edk2-platforms/{O6}/O6.fdf"
    experimental = f"custom/overlay-experimental-uefi-settings/edk2-platforms/{O6}/O6.fdf"
    if layout == "overlay":
        if tree.entries.get(ordinary, Entry("", "")).mode != "100644":
            problems.append(f"{ordinary}: expected regular custom FDF")
        if tree.entries.get(experimental, Entry("", "")).mode != "120000":
            problems.append(f"{experimental}: expected symlink to ordinary custom FDF")
        else:
            try:
                if tree.resolve(experimental) != ordinary:
                    problems.append(f"{experimental}: points away from ordinary custom FDF")
            except ValueError as exc:
                problems.append(f"{experimental}: {exc}")
        active = ordinary
    else:
        if ordinary in tree.entries:
            problems.append(f"{ordinary}: unexpected ordinary FDF for {edk2}")
        if tree.entries.get(experimental, Entry("", "")).mode != "100644":
            problems.append(f"{experimental}: expected regular experimental FDF")
        active = experimental
    data = _required(tree, active, problems)
    if not data:
        return
    if "1.2.4" in data:
        problems.append(f"{active}: stale 1.2.4 release label")
    defines = re.findall(r"^\s*DEFINE SKY1_BL33_UEFI_FD_(SIZE|BLOCKS)\s*=\s*(0x[0-9a-fA-F]+)",
                         data, re.MULTILINE)
    # DEBUG occupies the first pair; the production branch occupies the second.
    expected = [("SIZE", 0x400000), ("BLOCKS", 0x400),
                ("SIZE", release_size), ("BLOCKS", release_size // 0x1000)]
    if [(key, int(value, 16)) for key, value in defines] != expected:
        problems.append(f"{active}: wrong DEBUG/RELEASE BL33 FDF size or block count")


def _smmu_hooks(tree: GitTree, problems: list[str]) -> None:
    makefile = _required(tree, "src/Makefile", problems)
    for token in ("PCIE_SMMU_IORT_VALIDATOR :=", "$(PCIE_SMMU_IORT_VALIDATOR) \\",
                  'python3 "$(PCIE_SMMU_IORT_VALIDATOR)"',
                  '--expect "$(if $(filter TRUE,$(ENABLE_FIRMWARE_FIXES_NORMALIZED)),enabled,disabled)"'):
        if token not in makefile:
            problems.append(f"src/Makefile: missing PCIe SMMU IORT validator hook: {token}")
    for kind in ("ASLPP", "ASLCC", "VFRPP"):
        for target in ("DEBUG", "RELEASE"):
            if not re.search(rf"{target}_GCC(?:5)?_AARCH64_{kind}_FLAGS[^\n]*-DENABLE_FIRMWARE_FIXES=1", makefile):
                problems.append(f"src/Makefile: missing {target} {kind} firmware-fixes compiler flag")
    for variant in ("overlay", "overlay-experimental-uefi-settings"):
        path = f"custom/{variant}/{PCIE_MENU}/PcieConfig.hfr"
        data = _required(tree, path, problems)
        if not re.search(r"#ifdef ENABLE_FIRMWARE_FIXES\s+oneof varid\s*=\s*RadxaSetupVar\.PcieDeviceModel", data):
            problems.append(f"{path}: missing gated PCIe device-model UI")


def _asl_without_comments(data: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", data, flags=re.DOTALL)


def _device_body(data: str, name: str) -> str:
    match = re.search(rf"\bDevice\s*\(\s*{re.escape(name)}\s*\)\s*\{{", data)
    if match is None:
        return ""
    depth = 1
    for position in range(match.end(), len(data)):
        if data[position] == "{":
            depth += 1
        elif data[position] == "}":
            depth -= 1
            if depth == 0:
                return data[match.end():position]
    return ""


def _usb_mux1_pins(tree: GitTree, problems: list[str]) -> None:
    """Every custom O6 MUX1 USB VBUS consumer needs an IOMUX producer."""
    consumer_path = f"{O6_ACPI}/UsbPwr.asl"
    # No custom USB power overlay means there are no overlay consumers to
    # constrain; this check does not prescribe whether that overlay exists.
    if consumer_path not in tree.entries:
        return
    consumer_data = _asl_without_comments(_required(tree, consumer_path, problems))
    consumers = set()
    for call in re.findall(r"\bPinGroupFunction\s*\(([^)]*)\)", consumer_data):
        args = [part.strip() for part in call.split(",")]
        if (len(args) >= 6 and args[2].strip('"').endswith(".MUX1") and
                args[5] == "ResourceConsumer"):
            name = args[4].strip('"')
            if re.fullmatch(r"usb_drive_vbus\d+", name):
                consumers.add(name)
    if not consumers:
        return
    producer_path = f"{O6_ACPI}/RadxaO6Iomux.asl"
    iomux = _asl_without_comments(_required(tree, producer_path, problems))
    mux1 = _device_body(iomux, "MUX1")
    producers = set(re.findall(
        r'\bPinGroup\s*\(\s*"([^"]+)"\s*,\s*ResourceProducer\b', mux1))
    for name in sorted(consumers - producers):
        problems.append(f"{producer_path}: MUX1 lacks ResourceProducer PinGroup {name} required by {consumer_path}")


def validate(repo: Path, revision: str, edk2: str, radxa: str) -> list[str]:
    """Return all structural problems; reject unsupported layouts explicitly."""
    if (edk2, radxa) not in SUPPORTED_PROFILES:
        return [f"unsupported structural profile: edk2-{edk2}/radxa-{radxa}"]
    tree = GitTree(repo, revision)
    problems: list[str] = []
    for path, entry in tree.entries.items():
        if entry.mode == "120000" and (path.startswith("custom/overlay/") or
                                       path.startswith("custom/overlay-experimental-uefi-settings/")):
            try:
                tree.resolve(path)
            except ValueError as exc:
                problems.append(f"{path}: {exc}")
    _smbios(tree, problems)
    _cppc(tree, problems)
    _release_fdf(tree, edk2, radxa, problems)
    _smmu_hooks(tree, problems)
    _usb_mux1_pins(tree, problems)
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--revision", required=True, help="source commit/tree or ref")
    parser.add_argument("--edk2", required=True)
    parser.add_argument("--radxa", required=True)
    args = parser.parse_args()
    try:
        problems = validate(args.repo, args.revision, args.edk2, args.radxa)
    except (subprocess.CalledProcessError, ValueError) as exc:
        problems = [str(exc)]
    for problem in problems:
        print(problem)
    if problems:
        return 1
    print(f"structural preflight passed: edk2-{args.edk2}/radxa-{args.radxa} {args.revision}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
