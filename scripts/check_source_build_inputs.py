#!/usr/bin/env python3
"""Check module overlays in every retained Unofficial source tree."""

from __future__ import annotations

import posixpath
import re
from pathlib import Path

from reconstruction_common import for_each_ref, main_wrapper, ReconstructionError, tree_id
from source_lifecycle import tree_entries
from source_porting import git_blob_bytes_batch


ROOT = Path(__file__).resolve().parents[1]
OVERLAYS = ("custom/overlay", "custom/overlay-experimental-uefi-settings")
PACKAGE_ROOTS = ("src/edk2", "src/edk2-platforms", "src/edk2-non-osi", "src/edk2-platforms/Silicon/Intel")
SMBIOS_OVERLAY = "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/SmbiosMiscLib/SmbiosMiscLib.c"
SMBIOS_HEADER = "src/edk2/MdePkg/Include/IndustryStandard/SmBios.h"
CONFIG_MANAGER = "src/edk2-platforms/Platform/CIX/Sky1/Drivers/ConfigurationManagerDxe/ConfigurationManager.c"
CPU_GENERATOR = "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/Acpi/CIX/AcpiSsdtCpuTopologyLibCIX/SsdtCpuTopologyGenerator.c"
ACPI_NAMESPACE = "src/edk2-platforms/Platform/CIX/Sky1/Include/Library/AcpiNameSpaceObjects.h"
COMMON_NAMESPACE = "src/edk2/DynamicTablesPkg/Include/ArchCommonNameSpaceObjects.h"
# Regression contracts for focused fixes missed by vendor-line checkpoints.
# The audit in docs/src/source-checkpoint-maintenance.md records applicability.
BUILD_FIXES = (
    ("5f36bab487", "scripts/run_in_buildbox.sh", b'runtime pull --platform "$container_platform" "$container_image"', False),
    ("65f3abc664", "scripts/run_in_buildbox.sh", b'${git_objects}/info/alternates', False),
    ("24ef31676a", "scripts/run_in_buildbox.sh", b'"${git_common_dir_real}/"*)', False),
    ("57c8f42fe3", "scripts/firmware_metadata_audit.py", b'[A-Za-z0-9_][A-Za-z0-9_.-]*', False),
    ("052459dd2b/strict-bison", "scripts/ensure_iasl.sh", b"'YFLAGS=-y -Werror -Wno-yacc' NOWERROR=FALSE >&2", True),
    ("stored-version-header", "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionDxe.c", b"STR (UEFI_FW_VERSION),", False),
    ("release-logging-only", "custom/release-logging/Library/DebugLib.h", b"#error Custom RELEASE logging requires MDEPKG_NDEBUG and NDEBUG", False),
    ("autogen-warnings-fatal", "src/Makefile", b"build_extra_defines+=(-w)", False),
    ("release-logging-include", "src/Makefile", b'export PACKAGES_PATH="$$WORKSPACE/logging-overlay:$$PACKAGES_PATH"', False),
    ("custom-asl-without-fixes", "src/Makefile", b'ASLPP_FLAGS   = DEF(GCC_ASLPP_FLAGS) -I$(CUSTOM_OVERLAY_ROOT)', False),
)


def missing_build_fixes(contents: dict[str, bytes]) -> list[str]:
    return [f"missing build fix {commit} in {path}" for commit, path, marker, optional in BUILD_FIXES
            if not (optional and path not in contents) and marker not in contents.get(path, b"")]


def autogen_library_problems(imported: bytes, custom: bytes, descriptor: bytes) -> list[str]:
    expected = re.sub(rb"(LIBRARY_CLASS\s*=\s*)NULL\b", rb"\1LzmaDecompressLib",
                      imported.replace(b"\r\n", b"\n"))
    problems = []
    if custom.replace(b"\r\n", b"\n").strip() != expected.strip():
        problems.append("custom LZMA INF must change only the selected source library class")
    if not re.search(rb"CONSTRUCTOR\s*=\s*LzmaDecompressLibConstructor", custom):
        problems.append("custom LZMA INF must retain its decompressor constructor")
    if re.search(rb"(?m)^\s*DpuDxe\s*\|", descriptor):
        problems.append("DpuDxe is a driver, not a library-class implementation")
    return problems


def missing_toolchain(makefile: bytes, tools_definition: bytes) -> list[str]:
    selected = set(re.findall(rb"build\s+-a\s+AARCH64\s+-t\s+(\w+)", makefile))
    return [f"selected toolchain {tag.decode()} has no AARCH64 compiler path in tools_def.template"
            for tag in sorted(selected)
            if not re.search(rb"(?m)^\s*\*_" + re.escape(tag) + rb"_AARCH64_CC_PATH\s*=", tools_definition)]


def missing_tool_definitions(makefile: bytes, tools_definition: bytes) -> list[str]:
    """Check generated overrides, including generic macros shared by GCC tags."""
    required = set(re.findall(rb"\bDEF\((\w+)\)", makefile))
    defined = set(re.findall(rb"(?m)^\s*DEFINE\s+(\w+)\s*=", tools_definition))
    return [f"generated tools_def override references undefined macro {name.decode()}"
            for name in sorted(required - defined)]


def missing_acpi_headers(paths: set[str], tables: dict[str, str]) -> list[str]:
    """Custom tables must not silently assume headers from a newer EDK2."""
    headers = {path.split('/Include/', 1)[1] for path in paths if '/Include/' in path}
    # Vendor package DEC files also expose their root for Include/Foo.h forms.
    headers.update('Include/' + header for header in tuple(headers))
    return [f"{path}: missing ACPI table header {header}"
            for path, source in sorted(tables.items())
            for header in sorted(set(re.findall(r'^\s*#include\s+<([^>]+)>', source, re.M)) - headers)
            if posixpath.normpath(posixpath.join(posixpath.dirname(path), header)) not in paths]


def flattened_overlay_mirrors(contents: dict[str, bytes]) -> list[str]:
    """A Git symlink blob must never become a regular source file."""
    return [f"regular overlay contains symlink target text instead of source: {path}"
            for path, content in sorted(contents.items())
            if re.fullmatch(rb'(?:\.\./)+[A-Za-z0-9_./-]+', content.strip())]


def missing_smbios_cache_types(overlay: bytes, header: bytes) -> list[str]:
    """The Type 7 cache fields changed from integers to structs in 202511."""
    required = set(re.findall(rb"\bSMBIOS_CACHE_SIZE(?:_2)?\b", overlay))
    declared = set(re.findall(rb"}\s*(SMBIOS_CACHE_SIZE(?:_2)?)\s*;", header))
    return [f"SmbiosMiscLib uses {name.decode()} absent from this EDK2 SmBios.h"
            for name in sorted(required - declared)]


def missing_package_declarations(paths: set[str], infs: dict[str, str]) -> list[str]:
    roots = PACKAGE_ROOTS + tuple(f"{overlay}/{component}" for overlay in OVERLAYS
                                  for component in ("edk2", "edk2-platforms"))
    problems = []
    for path, text in infs.items():
        in_packages = False
        for number, line in enumerate(text.splitlines(), 1):
            line = line.split("#", 1)[0].strip()
            if line.startswith("["):
                in_packages = line.lower().startswith("[packages")
            elif in_packages and re.fullmatch(r"[\w/.-]+\.dec", line):
                if not any(f"{root}/{line}" in paths for root in roots):
                    problems.append(f"{path}:{number}: missing package declaration: {line}")
    return problems


def missing_wrapper_dependencies(infs: dict[str, str], sources: dict[str, str]) -> list[str]:
    """A wrapper including a maintained C file also needs its INF dependencies."""
    sections = {"packages", "libraryclasses", "guids", "protocols", "ppis"}

    def entries(text: str) -> set[tuple[str, str]]:
        result = set()
        section = ""
        for line in text.splitlines():
            line = line.split("#", 1)[0].strip()
            if line.startswith("["):
                section = line.strip("[]").split(".", 1)[0].lower()
            elif line and section in sections:
                result.add((section, line))
        return result

    problems = []
    # PlatformConfigDxe keeps the imported C implementation and vendor forms.
    # Dynamic-vs-fixed access can intentionally differ, but each PCD must exist.

    def pcds(text: str) -> set[str]:
        return set(re.findall(r"(?m)^\s*(g\w+\.Pcd\w+)\s*(?:#.*)?$", text))

    for path, descriptor in infs.items():
        if path.startswith("custom/overlay-experimental-uefi-settings/") and path.endswith("/PlatformConfigDxe/PlatformConfigDxe.inf"):
            imported = "src/" + path.split("/", 2)[2]
            for entry in sorted(pcds(infs.get(imported, "")) - pcds(descriptor)):
                problems.append(f"{path}: imported configuration UI requires {entry}")
    for wrapper, text in sources.items():
        for included in re.findall(r'^\s*#include "([^"]+\.c)"', text, re.M):
            target = posixpath.normpath(posixpath.join(posixpath.dirname(wrapper), included))
            for inf, descriptor in infs.items():
                if posixpath.dirname(inf) != posixpath.dirname(wrapper):
                    continue
                original = posixpath.join(posixpath.dirname(target), posixpath.basename(inf))
                if original not in infs:
                    continue
                for section, entry in sorted(entries(infs[original]) - entries(descriptor)):
                    problems.append(f"{inf}: included source {target} requires [{section}] {entry}")
    return problems


def missing_lto_library(paths: set[str], makefile: bytes) -> list[str]:
    """EDK2 moved its AArch64 LTO archive out of ArmPkg in 202408."""
    problems = []
    for directory in ("ArmPkg/Library/GccLto", "BaseTools/Bin/GccLto"):
        if directory.encode() not in makefile:
            continue
        if f"src/edk2/{directory}/liblto-aarch64.a" not in paths:
            problems.append(f"AArch64 LTO linker path {directory} has no support archive")
    return problems


def missing_configuration_manager_types(source: bytes, common: bytes, cix: bytes) -> list[str]:
    problems = []
    for name, header in ((b"CM_ARCH_COMMON_CPC_INFO", common), (b"CIX_AML_PSD_INFO", cix)):
        if name in source and name not in header:
            problems.append(f"ConfigurationManager uses {name.decode()} absent from its namespace headers")
    if b"CIX_AML_PSD_INFO" in cix and re.search(rb"\bAML_PSD_INFO\b", source):
        problems.append("ConfigurationManager uses upstream AML_PSD_INFO for the vendor PSD initializer")
    return problems


def missing_board_table_inputs(paths: set[str]) -> list[str]:
    """Board table overlays must retain the selected release's ASL inputs."""
    missing = []
    for board in ("O6", "O6N"):
        directory = f"edk2-platforms/Platform/Radxa/Orion/{board}/Drivers/AcpiPlatfomTables/"
        overlay = "custom/overlay/" + directory
        if overlay + "AcpiPlatfomTables.inf" not in paths:
            continue
        for path in sorted(paths):
            if path.startswith("src/" + directory) and path.endswith((".asl", ".asl.template", ".h")):
                counterpart = "custom/overlay/" + path[len("src/"):]
                if counterpart not in paths:
                    missing.append(f"missing board table input: {counterpart}")
    return missing


def unbalanced_asl_conditionals(text: str) -> list[str]:
    """Catch damaged conditional blocks before compiling a rendered board."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m[0].count("\n"), text, flags=re.S)
    stack = []
    problems = []
    for number, line in enumerate(text.splitlines(), 1):
        match = re.match(r"\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b", line)
        if not match:
            continue
        directive = match[1]
        if directive in {"if", "ifdef", "ifndef"}:
            stack.append((number, False))
        elif not stack:
            problems.append(f"line {number}: #{directive} without an opening conditional")
        elif directive == "endif":
            stack.pop()
        elif stack[-1][1]:
            problems.append(f"line {number}: #{directive} after #else")
        elif directive == "else":
            stack[-1] = (stack[-1][0], True)
    problems.extend(f"line {number}: unterminated conditional" for number, _ in stack)
    return problems


def missing_module_infs(paths: set[str], overlay: str, libraries: set[str] | None = None) -> list[str]:
    """GenMake resolves MODULE_DIR separately from the module's INF pathname.

    An overlay directory containing one INF can therefore shadow another INF
    in the same imported directory. Checking for just *any* INF misses this.
    """
    directories = set()
    for path in paths:
        if not path.startswith(overlay + "/"):
            continue
        parent = posixpath.dirname(path)
        while parent != overlay:
            directories.add(parent)
            parent = posixpath.dirname(parent)
    missing = []
    for path in sorted(paths):
        # Libraries do not generate FFS UI/version sections or their INF
        # prerequisites. Their sibling INF need not be mirrored for this check.
        if libraries and path in libraries:
            continue
        if not path.startswith("src/edk2") or not path.lower().endswith(".inf"):
            continue
        counterpart = overlay + "/" + path[len("src/"):]
        if posixpath.dirname(counterpart) in directories and counterpart not in paths:
            missing.append(counterpart)
    return missing


def missing_platform_inputs(paths: set[str], descriptors: dict[str, str], overlays: tuple[str, ...]) -> list[str]:
    """Check unconditional literal dependencies reachable from supported boards.

    This deliberately does not pretend to evaluate EDK2 expressions. Conditional
    and macro-dependent inputs are exercised by the actual firmware build jobs.
    """
    roots = tuple(f"{overlay}/{component}" for overlay in reversed(overlays)
                  for component in ("edk2", "edk2-platforms")) + PACKAGE_ROOTS

    def resolve(name: str) -> str | None:
        return next((f"{root}/{name}" for root in roots if f"{root}/{name}" in paths), None)

    pending = [f"Platform/Radxa/Orion/{board}/{board}.dsc" for board in ("O6", "O6N")]
    pending.extend(f"Platform/Radxa/Orion/{board}/{board}.fdf" for board in ("O6", "O6N")
                   if resolve(f"Platform/Radxa/Orion/{board}/{board}.fdf") is not None)
    visited = set()
    problems = []
    while pending:
        name = pending.pop()
        path = resolve(name)
        if path is None:
            problems.append(f"missing platform input: {name}")
            continue
        if path in visited:
            continue
        visited.add(path)
        depth = 0
        for number, line in enumerate(descriptors.get(path, "").splitlines(), 1):
            line = line.split("#", 1)[0].strip()
            if re.match(r"!if(?:n?def)?\b", line, re.IGNORECASE):
                depth += 1
            elif re.match(r"!endif\b", line, re.IGNORECASE):
                depth -= 1
            if depth or "$" in line:
                continue
            include = re.fullmatch(r"!include\s+([\w/.-]+)", line, re.IGNORECASE)
            if include:
                pending.append(include[1])
            for target in re.findall(r"(?:^|[\s|])([\w/.-]+\.inf)\b", line):
                if resolve(target) is None:
                    problems.append(f"{path}:{number}: missing platform input: {target}")
    return sorted(set(problems))


def source_input_problems(repo: Path, ref: str) -> list[str]:
    entries = tree_entries(repo, ref, ("src", "scripts", "custom/release-logging", *OVERLAYS))
    paths = set(entries)
    regular_overlays = {path: entry for path, entry in entries.items()
                        if path.startswith('custom/') and entry.mode != '120000'
                        and path.endswith(('.c', '.h', '.aslc', '.inf', '.dsc', '.dsc.inc', '.fdf', '.fdf.inc'))}
    overlay_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in regular_overlays.values()))
    infs = {path: entry for path, entry in entries.items() if path.startswith("src/edk2") and path.lower().endswith(".inf")}
    inf_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in infs.values()))
    libraries = {path for path, entry in infs.items() if re.search(rb"(?m)^\s*LIBRARY_CLASS\s*=", inf_blobs[entry.object_id])}
    problems = [f"missing overlay module: {path}" for overlay in OVERLAYS for path in missing_module_infs(paths, overlay, libraries)]
    problems.extend(flattened_overlay_mirrors({
        path: overlay_blobs[entry.object_id] for path, entry in regular_overlays.items()
    }))
    problems.extend(missing_board_table_inputs(paths))
    asl_entries = {path: entry for path, entry in entries.items()
                   if entry.mode != "120000" and path.endswith((".asl", ".asl.template"))
                   and "/edk2-platforms/" in path and ("/CIX/" in path or "/Radxa/" in path)}
    asl_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in asl_entries.values()))
    for path, entry in asl_entries.items():
        problems.extend(f"{path}: {error}" for error in unbalanced_asl_conditionals(asl_blobs[entry.object_id].decode("utf-8-sig")))
    fix_entries = {path: entries[path] for _, path, _, _ in BUILD_FIXES if path in entries}
    fix_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in fix_entries.values()))
    problems.extend(missing_build_fixes({path: fix_blobs[entry.object_id] for path, entry in fix_entries.items()}))
    lzma = "edk2/MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf"
    contracts = ("src/" + lzma, "custom/overlay/" + lzma,
                 "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc")
    if all(path in entries for path in contracts):
        values = git_blob_bytes_batch(repo, (entries[path].object_id for path in contracts))
        problems.extend(autogen_library_problems(*(values[entries[path].object_id] for path in contracts)))
    else:
        problems.append("missing custom AutoGen library repair inputs")
    tool_paths = ("src/Makefile", "src/edk2/BaseTools/Conf/tools_def.template")
    if all(path in entries for path in tool_paths):
        tool_blobs = git_blob_bytes_batch(repo, (entries[path].object_id for path in tool_paths))
        problems.extend(missing_toolchain(*(tool_blobs[entries[path].object_id] for path in tool_paths)))
        problems.extend(missing_tool_definitions(*(tool_blobs[entries[path].object_id] for path in tool_paths)))
        problems.extend(missing_lto_library(paths, tool_blobs[entries["src/Makefile"].object_id]))
    if all(path in entries for path in (SMBIOS_OVERLAY, SMBIOS_HEADER)):
        smbios_blobs = git_blob_bytes_batch(repo, (entries[path].object_id for path in (SMBIOS_OVERLAY, SMBIOS_HEADER)))
        problems.extend(missing_smbios_cache_types(*(smbios_blobs[entries[path].object_id]
                                                   for path in (SMBIOS_OVERLAY, SMBIOS_HEADER))))
    for module in (CONFIG_MANAGER, CONFIG_MANAGER.replace("src/", "custom/overlay/", 1), CPU_GENERATOR):
        if module not in entries:
            continue
        cm_paths = (module, COMMON_NAMESPACE, ACPI_NAMESPACE)
        cm_blobs = git_blob_bytes_batch(repo, (entries[path].object_id for path in cm_paths if path in entries))
        problems.extend(f"{module}: {problem}" for problem in missing_configuration_manager_types(*(
            cm_blobs[entries[path].object_id] if path in entries else b"" for path in cm_paths
        )))
    links = {path: entry for path, entry in entries.items() if entry.mode == "120000" and path.startswith("custom/")}
    blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in links.values()))
    resolved_links = {}
    for path in links:
        target = path
        seen = set()
        while target in links and target not in seen:
            seen.add(target)
            text = blobs[links[target].object_id].decode("utf-8")
            target = posixpath.normpath(posixpath.join(posixpath.dirname(target), text))
        if target in seen or target.startswith(("/", "../")) or (target not in paths and not any(p.startswith(target + "/") for p in paths)):
            problems.append(f"unresolvable overlay symlink: {path} -> {target}")
        else:
            resolved_links[path] = target
    descriptor_entries = {
        path: entries[resolved_links.get(path, path)] for path in paths
        if path.endswith((".dsc", ".dsc.inc", ".fdf", ".fdf.inc"))
        and (entries[path].mode != "120000" or resolved_links.get(path) in entries)
    }
    tables = {path: entries[resolved_links.get(path, path)] for path in paths
              if path.startswith('custom/') and path.endswith('.aslc')
              and (entries[path].mode != '120000' or resolved_links.get(path) in entries)}
    table_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in tables.values()))
    problems.extend(missing_acpi_headers(paths, {
        path: table_blobs[entry.object_id].decode('utf-8-sig') for path, entry in tables.items()
    }))
    descriptor_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in descriptor_entries.values()))
    descriptors = {path: descriptor_blobs[entry.object_id].decode("utf-8-sig") for path, entry in descriptor_entries.items()}
    for overlays in (OVERLAYS[:1], OVERLAYS):
        problems.extend(missing_platform_inputs(paths, descriptors, overlays))
    platform_infs = {
        path: entries[resolved_links.get(path, path)] for path in paths
        if path.endswith(".inf") and "/edk2-platforms/Platform/" in path
        and ("/CIX/" in path or "/Radxa/" in path)
        and (entries[path].mode != "120000" or resolved_links.get(path) in entries)
    }
    package_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in platform_infs.values()))
    inf_text = {path: package_blobs[entry.object_id].decode("utf-8-sig") for path, entry in platform_infs.items()}
    problems.extend(missing_package_declarations(paths, inf_text))
    wrappers = {path: entry for path, entry in entries.items()
                if path.startswith("custom/overlay-experimental-uefi-settings/")
                and path.endswith(".c") and entry.mode != "120000"}
    wrapper_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in wrappers.values()))
    problems.extend(missing_wrapper_dependencies(inf_text, {
        path: wrapper_blobs[entry.object_id].decode("utf-8-sig") for path, entry in wrappers.items()
    }))
    return sorted(set(problems))


def main() -> None:
    refs = for_each_ref(ROOT, "source/unofficial")
    checked = {}
    problems = []
    for ref in refs:
        tree = tree_id(ROOT, ref)
        if tree not in checked:
            checked[tree] = source_input_problems(ROOT, ref)
            entries = tree_entries(ROOT, ref, ("src/scripts", "scripts/debug_build_policy.py"))
            # Packaging executes inside rendered trees as well as from build.
            # Every retained checkpoint must carry the same mandatory verifier.
            for source, caller in (("scripts/debug_build_policy.py", "scripts/debug_build_policy.py"),
                                   ("src/scripts/firmware_chain.py", "scripts/firmware_chain.py"),
                                   ("src/scripts/validate_firmware_chain.py", "scripts/validate_firmware_chain.py"),
                                   ("src/scripts/check_release_debug.py", "scripts/check_release_debug.py"),
                                   ("src/scripts/prepare_release_logging.py", "scripts/prepare_release_logging.py"),
                                   ("src/scripts/firmware-trust.json", "config/firmware-trust.json")):
                entry = entries.get(source)
                if entry is None:
                    checked[tree].append(f"missing mandatory build validation input: {source}")
                else:
                    content = git_blob_bytes_batch(ROOT, [entry.object_id])[entry.object_id]
                    if content != (ROOT / caller).read_bytes():
                        checked[tree].append(f"build validation input differs from build branch: {source}")
        problems.extend(f"{ref}: {problem}" for problem in checked[tree])
    if problems:
        raise ReconstructionError("source build-input checks failed:\n" + "\n".join(problems))
    print(f"Source build inputs passed for {len(refs)} refs / {len(checked)} distinct trees")


if __name__ == "__main__":
    main_wrapper(main)
