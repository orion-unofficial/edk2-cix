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
# Regression contracts for focused fixes missed by vendor-line checkpoints.
# The audit in docs/src/source-checkpoint-maintenance.md records applicability.
BUILD_FIXES = (
    ("5f36bab487", "scripts/run_in_buildbox.sh", b'runtime pull --platform "$container_platform" "$container_image"', False),
    ("65f3abc664", "scripts/run_in_buildbox.sh", b'${git_objects}/info/alternates', False),
    ("24ef31676a", "scripts/run_in_buildbox.sh", b'"${git_common_dir_real}/"*)', False),
    ("57c8f42fe3", "scripts/firmware_metadata_audit.py", b'[A-Za-z0-9_][A-Za-z0-9_.-]*', False),
    ("052459dd2b", "scripts/ensure_iasl.sh", b'make -C "${source_root}/generate/unix" iasl >&2', True),
)


def missing_build_fixes(contents: dict[str, bytes]) -> list[str]:
    return [f"missing build fix {commit} in {path}" for commit, path, marker, optional in BUILD_FIXES
            if not (optional and path not in contents) and marker not in contents.get(path, b"")]


def missing_toolchain(makefile: bytes, tools_definition: bytes) -> list[str]:
    selected = set(re.findall(rb"build\s+-a\s+AARCH64\s+-t\s+(\w+)", makefile))
    return [f"selected toolchain {tag.decode()} has no AARCH64 compiler flags in tools_def.template"
            for tag in sorted(selected)
            if not re.search(rb"(?m)^DEFINE\s+" + re.escape(tag) + rb"_AARCH64_CC_FLAGS\s*=", tools_definition)]


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
    entries = tree_entries(repo, ref, ("src", "scripts", *OVERLAYS))
    paths = set(entries)
    infs = {path: entry for path, entry in entries.items() if path.startswith("src/edk2") and path.lower().endswith(".inf")}
    inf_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in infs.values()))
    libraries = {path for path, entry in infs.items() if re.search(rb"(?m)^\s*LIBRARY_CLASS\s*=", inf_blobs[entry.object_id])}
    problems = [f"missing overlay module: {path}" for overlay in OVERLAYS for path in missing_module_infs(paths, overlay, libraries)]
    fix_entries = {path: entries[path] for _, path, _, _ in BUILD_FIXES if path in entries}
    fix_blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in fix_entries.values()))
    problems.extend(missing_build_fixes({path: fix_blobs[entry.object_id] for path, entry in fix_entries.items()}))
    tool_paths = ("src/Makefile", "src/edk2/BaseTools/Conf/tools_def.template")
    if all(path in entries for path in tool_paths):
        tool_blobs = git_blob_bytes_batch(repo, (entries[path].object_id for path in tool_paths))
        problems.extend(missing_toolchain(*(tool_blobs[entries[path].object_id] for path in tool_paths)))
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
        if path.endswith((".dsc", ".dsc.inc"))
        and (entries[path].mode != "120000" or resolved_links.get(path) in entries)
    }
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
    problems.extend(missing_package_declarations(paths, {
        path: package_blobs[entry.object_id].decode("utf-8-sig") for path, entry in platform_infs.items()
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
        problems.extend(f"{ref}: {problem}" for problem in checked[tree])
    if problems:
        raise ReconstructionError("source build-input checks failed:\n" + "\n".join(problems))
    print(f"Source build inputs passed for {len(refs)} refs / {len(checked)} distinct trees")


if __name__ == "__main__":
    main_wrapper(main)
