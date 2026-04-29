#!/usr/bin/env python3
"""Integrate new upstream or vendor source refs into the reconstruction model."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from reconstruction_common import ReconstructionError, check_immutable_refs, git, main_wrapper, ref_exists, repo_root, truthy


HELP = """integrate-source-release

Supported forms:
  make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202602 WRITE=1
  make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7 WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2 WRITE=1
  make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 WRITE=1

Required variables:
  TYPE=upstream|vendor
  COMPONENT=edk2|edk2-platforms|edk2-non-osi|tf-a|op-tee when TYPE=upstream
  VENDOR=radxa|cix when TYPE=vendor

Optional variables:
  RELEASE=<release-or-ref>  Release tag/version to integrate.
  EDK2_BASE=<release>       Vendor base marker for Radxa deltas.
  REF=<object-id-or-ref>    Explicit object to use instead of a configured remote tag.
  WRITE=1                   Required before refs are created or advanced.
  V=1                       Print delegated git operations.

Without WRITE=1 this command validates inputs and prints the operation it would perform.
Only this command is allowed to create or advance immutable source refs.
"""

UPSTREAM_COMPONENTS = {"edk2", "edk2-platforms", "edk2-non-osi", "tf-a", "op-tee"}
VENDORS = {"radxa", "cix"}

UPSTREAM_REMOTES = {
    "edk2": "https://github.com/tianocore/edk2.git",
    "edk2-platforms": "https://github.com/tianocore/edk2-platforms.git",
    "edk2-non-osi": "https://github.com/tianocore/edk2-non-osi.git",
    "tf-a": "https://github.com/ARM-software/arm-trusted-firmware.git",
    "op-tee": "https://github.com/OP-TEE/optee_os.git",
}

CIX_COMPONENTS = {
    "bios-superproject": {
        "remote": "https://github.com/cixtech/bios.git",
        "ref": "90f39f4469d39b3cd135ca8c5ae6400aca75b292",
        "target": "source/component/cix/1.2/bios-superproject",
    },
    "tf-a": {
        "remote": "https://github.com/cixtech/cix_opensource__arm-trusted-firmware.git",
        "ref": "114fb20577bcc4038025de4e12bca60e04dd5212",
        "target": "source/component/cix/1.2/tf-a",
    },
    "op-tee": {
        "remote": "https://github.com/cixtech/cix_opensource__tee__op-tee.git",
        "ref": "cc66640f3815da4defc50f72b66ae3bac97cd48a",
        "target": "source/component/cix/1.2/op-tee",
    },
}


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--type", dest="type_", default=os.environ.get("TYPE", ""))
    p.add_argument("--component", default=os.environ.get("COMPONENT", ""))
    p.add_argument("--vendor", default=os.environ.get("VENDOR", ""))
    p.add_argument("--release", default=os.environ.get("RELEASE", ""))
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
    p.add_argument("--ref", default=os.environ.get("REF", ""))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def upstream_target(component: str, release: str) -> str:
    if component.startswith("edk2"):
        return f"source/base/edk2/{component}/{release}"
    return f"source/base/arm/{component}/{release}"


def fetch_to_ref(repo: Path, remote: str, source: str, target: str, verbose: bool) -> None:
    if ref_exists(repo, target):
        raise ReconstructionError(f"target immutable ref already exists: {target}\nRefusing to move it without an explicit manifest-update workflow.")
    if verbose:
        print(f"fetch {remote} {source} -> {target}")
    git(repo, "fetch", "--no-tags", remote, f"{source}:refs/heads/{target}", capture=not verbose)


def validate(args: argparse.Namespace) -> list[str]:
    missing: list[str] = []
    if args.type_ not in {"upstream", "vendor"}:
        missing.append("TYPE=upstream|vendor")
    if args.type_ == "upstream" and args.component not in UPSTREAM_COMPONENTS:
        missing.append("COMPONENT=edk2|edk2-platforms|edk2-non-osi|tf-a|op-tee")
    if args.type_ == "vendor" and args.vendor not in VENDORS:
        missing.append("VENDOR=radxa|cix")
    if args.type_ == "upstream" and not (args.release or args.ref):
        missing.append("RELEASE=<release> or REF=<object-id-or-ref>")
    if args.type_ == "vendor" and args.vendor == "cix" and args.release not in {"1.2", "v1.2"}:
        missing.append("RELEASE=1.2")
    if args.type_ == "vendor" and args.vendor == "radxa" and not (args.release and args.edk2_base):
        missing.append("RELEASE=<radxa-release> EDK2_BASE=<edk2-release>")
    return missing


def main() -> None:
    args = parser().parse_args()
    missing = validate(args)
    if missing:
        print(HELP)
        raise SystemExit(2)
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    write = truthy(args.write)
    check_immutable_refs(repo, allow_manifest_update=write)

    operations: list[tuple[str, str, str]] = []
    if args.type_ == "upstream":
        release = args.release or args.ref
        target = upstream_target(args.component, release)
        source = args.ref or f"refs/tags/{release}"
        remote = UPSTREAM_REMOTES[args.component]
        operations.append((remote, source, target))
    elif args.vendor == "cix":
        for item in CIX_COMPONENTS.values():
            operations.append((item["remote"], item["ref"], item["target"]))
    else:
        target = f"source/delta/radxa/{args.release}/{args.edk2_base}"
        source = args.ref or "<materialized-vendor-ref>"
        operations.append(("local", source, target))

    if not write:
        print("dry run; set WRITE=1 to create refs")
        for remote, source, target in operations:
            print(f"  {remote} {source} -> {target}")
        return

    for remote, source, target in operations:
        if remote == "local":
            if source.startswith("<"):
                raise ReconstructionError("Radxa vendor integration requires REF=<local-ref-or-object> in WRITE mode")
            if ref_exists(repo, target):
                raise ReconstructionError(f"target immutable ref already exists: {target}")
            git(repo, "branch", target, source, capture=not verbose)
        else:
            fetch_to_ref(repo, remote, source, target, verbose)
    print("integration refs created; update config/refs metadata in the same change before committing")


if __name__ == "__main__":
    main_wrapper(main)
