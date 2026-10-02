#!/usr/bin/env python3
"""Bounded resolution of the reviewed 202408.01 / Radxa 1.3.1 source conflict.

The input trees, entire editable blobs, resolution parent and untouched tree
entries are verified. Other release pairs require a separate source review.
This host-side module never updates canonical refs or registers source metadata.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import subprocess

CAPSULE = "src/edk2/MdeModulePkg/Library/DxeCapsuleLibFmp/"
RESET = "src/edk2-platforms/Platform/CIX/Sky1/Library/ArmPsciResetSystemLib/"
PATHS = (RESET + "ArmPsciResetSystemLib.c", RESET + "ResetSystemLib.c",
         CAPSULE + "DxeCapsuleLib.c", CAPSULE + "DxeCapsuleRuntime.c")
INPUTS = {
    "old": ("source/vendor/radxa/1.2.1/edk2-stable202208",
            "28fe053174551017054a67772a42bf7ae51837de", "e6799abb209a02a3354357e21706d72c0110f382"),
    "new": ("source/port/radxa/1.2.1/edk2-stable202408.01",
            "3c6a55756a45ceac906e1a763ad8283c35821cf7", "49c544fe389d45d4b829b88f0bdfdfe81450bbca"),
    "source": ("source/vendor/radxa/1.3.1/edk2-stable202208",
               "8628b567985a6943897faf23aa2e392329bfb4b6", "b254bc89d11d3457d5ce1b132d7bdf3d09ebe704"),
}
BLOBS = {'src/edk2/MdeModulePkg/Library/DxeCapsuleLibFmp/DxeCapsuleLib.c': {'preimage_sha256': '22354ec8bfa09b5a890e7693ef65e7da462cb17a8697af12115051d23baa7edc',
                                                                    'output_sha256': '16aa7a4d1a19d5da4265899e92a224ece3db5b5d8171d05078000c44cd1db87e'},
 'src/edk2/MdeModulePkg/Library/DxeCapsuleLibFmp/DxeCapsuleRuntime.c': {'preimage_sha256': 'e9faa1423bbb2b8843824f50d0ec1c1cf8aa2d26c0ec6e645bfe7fafc7ee2ea0',
                                                                        'output_sha256': 'df30a72055dc8d385e640c6cba7e2c99e4013adf75e3c30a0fb7b017207daa19'},
 'src/edk2-platforms/Platform/CIX/Sky1/Library/ArmPsciResetSystemLib/ResetSystemLib.c': {'preimage_sha256': '4208cd0913220c02740d80a53e8f25b6b1d1f3f310cdfe51ba5a08c5d12decf8',
                                                                                         'output_sha256': '448ae300c2984cf8576025a3eded94844eeaf8553c84faf609201ac881efd1bc'},
 'src/edk2-platforms/Platform/CIX/Sky1/Library/ArmPsciResetSystemLib/ArmPsciResetSystemLib.c': {'preimage_sha256': '85d10467d25dcc866c533cf4012e103398c8b1a1894be8410b2b6abe0b6665c5',
                                                                                                'output_sha256': None}}


class RadxaSourceConflictError(ValueError):
    """The source conflict or journal is outside the reviewed bounds."""


def _canonical_markers(raw: bytes) -> bytes:
    return re.sub(rb"(?m)^(<<<<<<<|>>>>>>>) [^\n]+$", rb"\1 reviewed", raw)


def resolve_radxa13_source_conflict(blobs: dict[str, bytes]) -> dict[str, bytes | None]:
    """Resolve exactly four reviewed whole files, preserving all other bytes."""
    if set(blobs) != set(PATHS):
        raise RadxaSourceConflictError("expected exactly the four reviewed source paths")
    output: dict[str, bytes | None] = {}
    for path, raw in blobs.items():
        if not isinstance(raw, bytes):
            raise TypeError("source blobs must be bytes")
        if hashlib.sha256(_canonical_markers(raw)).hexdigest() != BLOBS[path]["preimage_sha256"]:
            raise RadxaSourceConflictError("reviewed whole-file preimage differs: " + path)
        if path == RESET + "ArmPsciResetSystemLib.c":
            output[path] = None
            continue
        if path.startswith(CAPSULE):
            # EDK2 owns the global in the common translation unit. Both INF
            # variants compile it; only the runtime variant compiles Runtime.c.
            resolved, count = re.subn(
                rb"(?m)^<<<<<<< [^\n]+\n(.*?)^=======\n.*?^>>>>>>> [^\n]+\n",
                lambda match: match[1], raw, flags=re.S)
            if count != 1:
                raise RadxaSourceConflictError("expected one capsule ownership conflict")
        else:
            # Transfer the vendor fix into the standard ResetSystemLib entry
            # points: successful EC reset waits, failed EC reset uses PSCI.
            resolved = raw.replace(b"  EC_PARAMS_FORCE_EC_RESET  Params;\n",
                                   b"  EC_PARAMS_FORCE_EC_RESET  Params;\n  EFI_STATUS                Status;\n", 1)
            resolved = resolved.replace(
                b'  ForceEcReset (&Params);\n\n  DEBUG ((DEBUG_INFO, "%a: force EC reset\\n", __FUNCTION__));\n  CpuDeadLoop ();\n',
                b'  Status = ForceEcReset (&Params);\n  if (!EFI_ERROR (Status)) {\n    DEBUG ((DEBUG_INFO, "%a: force EC reset\\n", __FUNCTION__));\n    CpuDeadLoop ();\n  }\n\n  ResetCold ();\n', 1)
        if hashlib.sha256(resolved).hexdigest() != BLOBS[path]["output_sha256"]:
            raise RadxaSourceConflictError("reviewed output differs: " + path)
        output[path] = resolved
    return output


def _git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True)
    if check and result.returncode:
        raise RadxaSourceConflictError(result.stderr.decode(errors="replace").strip())
    return result


def _text(repo: Path, *args: str) -> str:
    return _git(repo, *args).stdout.decode().strip()


def _blob(repo: Path, commit: str, path: str) -> bytes:
    return _git(repo, "show", commit + ":" + path).stdout


def validated_source_resolution(repo: Path, journal: Path, edk2: str, radxa: str) -> str:
    """Verify selected inputs, merge preimage, parent and the complete output tree.

    A journal is a receipt, never permission to accept an arbitrary PORT_REF.
    The private resolution ref must already exist. There are no ref mutations.
    """
    row = json.loads(journal.read_text())
    if not isinstance(row, dict) or not isinstance(row.get("inputs"), dict):
        raise RadxaSourceConflictError("source resolution journal has invalid structure")
    if (edk2, radxa) != ("202408.01", "1.3.1") or row.get("pair") != [edk2, radxa]:
        raise RadxaSourceConflictError("source resolution pair has not been reviewed")
    if row.get("stage") != "source" or row.get("paths") != list(PATHS):
        raise RadxaSourceConflictError("source resolution scope differs")
    oids = {}
    for role, (ref, original, tree) in INPUTS.items():
        inp = row["inputs"].get(role, {})
        if not isinstance(inp, dict):
            raise RadxaSourceConflictError("source input receipt has invalid structure: " + role)
        if inp.get("ref") != ref or inp.get("original_commit") != original or inp.get("normalized_tree") != tree:
            raise RadxaSourceConflictError("source input identity differs: " + role)
        selected = None
        for candidate in ("refs/heads/" + ref, "refs/remotes/origin/" + ref):
            result = _git(repo, "rev-parse", "--verify", "--quiet", candidate + "^{commit}", check=False)
            if result.returncode == 0:
                selected = result.stdout.decode().strip()
                break
        if selected != original:
            raise RadxaSourceConflictError("selected source ref changed: " + ref)
        oid = inp.get("normalized_commit", "")
        if not isinstance(oid, str) or not re.fullmatch(r"[0-9a-f]{40}", oid) or _text(repo, "rev-parse", oid + "^{tree}") != tree:
            raise RadxaSourceConflictError("normalized source preimage differs: " + role)
        oids[role] = oid
    conflict, resolved = row.get("conflict_commit", ""), row.get("resolution_commit", "")
    if not all(isinstance(oid, str) and re.fullmatch(r"[0-9a-f]{40}", oid) for oid in (conflict, resolved)):
        raise RadxaSourceConflictError("invalid resolution commit identity")
    if _text(repo, "rev-list", "--parents", "-n", "1", conflict).split() != [conflict]:
        raise RadxaSourceConflictError("conflict must be the preserved root handoff")
    message = _text(repo, "show", "-s", "--format=%B", conflict) + "\n"
    if (f"Source-Port-Input: {INPUTS['source'][0]}\n" not in message or
            f"Source-Port-New-Base: {INPUTS['new'][0]}\n" not in message or
            "Source-Port-Conflict-Stage: source\n" not in message):
        raise RadxaSourceConflictError("conflict message does not bind its source inputs")
    merge = _git(repo, "-c", "merge.renames=false", "merge-tree", "--write-tree",
                 "--merge-base=" + oids["old"], oids["new"], oids["source"], check=False)
    if merge.returncode != 1 or merge.stdout.decode().splitlines()[0] != _text(repo, "rev-parse", conflict + "^{tree}"):
        raise RadxaSourceConflictError("conflict tree does not reproduce from the reviewed inputs")
    if _text(repo, "rev-list", "--parents", "-n", "1", resolved).split() != [resolved, conflict]:
        raise RadxaSourceConflictError("resolution does not have the exact conflict parent")
    if _text(repo, "diff", "--name-only", conflict, resolved).splitlines() != sorted(PATHS):
        raise RadxaSourceConflictError("resolution changed paths outside the reviewed source files")
    expected = resolve_radxa13_source_conflict({p: _blob(repo, conflict, p) for p in PATHS})
    for path, data in expected.items():
        entry = _text(repo, "ls-tree", resolved, "--", path)
        if data is None:
            if entry:
                raise RadxaSourceConflictError("obsolete source file remains")
        elif not entry.startswith("100644 blob ") or _blob(repo, resolved, path) != data:
            raise RadxaSourceConflictError("source output bytes or mode differ: " + path)
    if row.get("resolution_tree") != _text(repo, "rev-parse", resolved + "^{tree}"):
        raise RadxaSourceConflictError("resolution tree receipt differs")
    ref = f"refs/heads/batch/resolutions/{edk2}-{radxa}-source"
    if row.get("resolution_ref") != ref or _text(repo, "rev-parse", "--verify", ref) != resolved:
        raise RadxaSourceConflictError("private source resolution ref differs")
    return resolved
