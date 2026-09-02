#!/usr/bin/env python3
"""Atomically publish a build metadata commit and its pending source refs."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import time
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    format_duration,
    git,
    load_ref_records,
    main_wrapper,
    repo_root,
    truthy,
)


HELP = """publish-source-update

Optional variables:
  SOURCE_REFS=<ref[,ref...]>
      Additional source branches or tags to validate for publication. Pending
      refs are inferred from the checked-out metadata and the remote, so this
      variable is normally unnecessary.
  REMOTE=<name>  Git remote to update. Default: origin.
  WRITE=0|1      Execute the atomic push. Without WRITE=1, use git push --dry-run.
  V=0|1          Print delegated Git operations.

The checked-out build branch must be clean, contain object/tree metadata matching
every pending source ref, and be either ahead of or identical to the remote
build branch. The identical case safely resumes a publication whose metadata
commit reached the remote before its source refs. Existing immutable source refs
may not move. Mutable Unofficial tips, compatibility refs, and compatibility
tags use an exact force-with-lease expectation. Git receives the build branch
and every inferred pending ref in one --atomic push.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP
    )
    p.add_argument("--source-refs", default=os.environ.get("SOURCE_REFS", ""))
    p.add_argument("--remote", default=os.environ.get("REMOTE", "origin"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def canonical_ref(value: str) -> str:
    ref = value.strip()
    if ref.startswith("source/"):
        ref = f"refs/heads/{ref}"
    if not ref.startswith(("refs/heads/source/", "refs/tags/source/")):
        raise ReconstructionError(f"not a source branch or tag: {value}")
    if ref.startswith(("refs/heads/source/cache/", "refs/heads/source/component/")):
        raise ReconstructionError(f"generated or obsolete source ref cannot be published: {value}")
    return ref


def metadata_ref(ref: str) -> str:
    if ref.startswith("refs/heads/"):
        return ref.removeprefix("refs/heads/")
    prefix = "refs/tags/source/unofficial/edk2/stable-"
    if ref.startswith(prefix):
        return f"source/unofficial/edk2-stable{ref.removeprefix(prefix)}"
    raise ReconstructionError(f"source tag has no metadata mapping: {ref}")


def mutable_ref(ref: str) -> bool:
    short = ref.removeprefix("refs/heads/")
    return (
        ref.startswith("refs/tags/source/unofficial/edk2/stable-")
        or (short.startswith("source/unofficial/") and short.endswith("/current"))
        or short.startswith("source/unofficial/edk2-stable")
    )


def remote_objects(repo: Path, remote: str, refs: list[str]) -> dict[str, str]:
    result = git(repo, "ls-remote", remote, *refs, check=False)
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise ReconstructionError(f"cannot query {remote}: {detail or 'git ls-remote failed'}")
    return {
        ref: oid
        for line in result.stdout.splitlines()
        if line.strip()
        for oid, ref in [line.split(None, 1)]
        if not ref.endswith("^{}")
    }


def local_commit(repo: Path, ref: str) -> str | None:
    result = git(repo, "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}", check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def compatibility_tag(ref: str) -> str | None:
    prefix = "refs/heads/source/unofficial/edk2-stable"
    if not ref.startswith(prefix):
        return None
    release = ref.removeprefix(prefix)
    return f"refs/tags/source/unofficial/edk2/stable-{release}" if release else None


def infer_pending_refs(
    repo: Path,
    records: list[dict],
    queried: dict[str, str],
) -> list[str]:
    refs: set[str] = set()
    for record in records:
        source_ref = str(record.get("ref", ""))
        if not source_ref.startswith("source/") or source_ref.startswith(
            ("source/cache/", "source/component/")
        ):
            continue
        ref = f"refs/heads/{source_ref}"
        recorded_object = str(record.get("object_id", ""))
        local = local_commit(repo, ref)
        if local is not None and local != recorded_object:
            raise ReconstructionError(
                f"{ref}: local ref {local} does not match build metadata {recorded_object}; "
                "refresh and commit source metadata before publication"
            )
        if queried.get(ref) != recorded_object:
            if local is None:
                raise ReconstructionError(
                    f"{ref}: remote does not match the build metadata and the local ref is unavailable; "
                    "fetch or reconstruct the source ref before publication"
                )
            refs.add(ref)
        tag = compatibility_tag(ref)
        if tag and queried.get(tag) != recorded_object:
            local_tag = local_commit(repo, tag)
            if local_tag is not None and local_tag != recorded_object:
                raise ReconstructionError(
                    f"{tag}: local tag {local_tag} does not match its compatibility branch "
                    f"{recorded_object}"
                )
            if local_tag is None:
                raise ReconstructionError(
                    f"{tag}: remote does not match the compatibility branch and the local tag is unavailable; "
                    "run `make refresh-source-metadata UPDATE_RELEASE_TAGS=1 WRITE=1` first"
                )
            refs.add(tag)
    return sorted(refs)


def verify_metadata(repo: Path, ref: str, records: list[dict]) -> str:
    recorded_ref = metadata_ref(ref)
    matches = [record for record in records if record.get("ref") == recorded_ref]
    if len(matches) != 1:
        raise ReconstructionError(
            f"{recorded_ref}: expected exactly one expanded config/refs-*.json record, found {len(matches)}"
        )
    commit = git(repo, "rev-parse", f"{ref}^{{commit}}").stdout.strip()
    tree = git(repo, "rev-parse", f"{ref}^{{tree}}").stdout.strip()
    record = matches[0]
    if str(record.get("object_id", "")) != commit or str(record.get("tree_id", "")) != tree:
        raise ReconstructionError(
            f"{recorded_ref}: build metadata does not match the selected ref "
            f"(object {record.get('object_id', '<missing>')} / tree {record.get('tree_id', '<missing>')}; "
            f"local {commit} / {tree})"
        )
    if ref.startswith("refs/tags/"):
        branch = f"refs/heads/{recorded_ref}"
        branch_commit = git(repo, "rev-parse", f"{branch}^{{commit}}").stdout.strip()
        if branch_commit != commit:
            raise ReconstructionError(f"{ref}: tag does not identify its recorded compatibility branch")
    return str(record["manifest"])


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    explicit_refs = {
        canonical_ref(item) for item in args.source_refs.split(",") if item.strip()
    }
    if git(repo, "symbolic-ref", "--quiet", "--short", "HEAD").stdout.strip() != "build":
        raise ReconstructionError("publish-source-update must run from the checked-out build branch")
    if git(repo, "status", "--porcelain").stdout.strip():
        raise ReconstructionError("working tree is dirty; commit the build metadata before publication")

    records = load_ref_records(repo)
    published_refs = {
        f"refs/heads/{record['ref']}"
        for record in records
        if str(record.get("ref", "")).startswith("source/")
        and not str(record["ref"]).startswith(("source/cache/", "source/component/"))
    }
    published_refs.update(
        tag
        for ref in tuple(published_refs)
        for tag in [compatibility_tag(ref)]
        if tag
    )
    local_build = git(repo, "rev-parse", "refs/heads/build").stdout.strip()
    queried = remote_objects(
        repo,
        args.remote,
        ["refs/heads/build", *sorted(published_refs | explicit_refs)],
    )
    inferred_refs = set(infer_pending_refs(repo, records, queried))
    refs = sorted(explicit_refs | inferred_refs)
    if inferred_refs:
        print(f"inferred {len(inferred_refs)} pending source ref(s) from build metadata")
    remote_build = queried.get("refs/heads/build")
    if not remote_build:
        raise ReconstructionError(f"{args.remote} has no build branch")
    build_pending = remote_build != local_build
    if build_pending:
        fetched = git(repo, "fetch", "--quiet", "--no-tags", args.remote, "refs/heads/build", check=False)
        if fetched.returncode != 0 or subprocess.run(
            ["git", "-C", str(repo), "merge-base", "--is-ancestor", "FETCH_HEAD", local_build],
            check=False,
        ).returncode != 0:
            raise ReconstructionError("local build is not a fast-forward of the remote build branch")
    else:
        print("remote build already contains the selected metadata commit; checking for source refs to resume")

    refspecs: list[str] = []
    leases: list[str] = []
    if build_pending:
        refspecs.append("refs/heads/build:refs/heads/build")
        leases.append(f"--force-with-lease=refs/heads/build:{remote_build}")
    matched_refs = 0
    for ref in refs:
        git(repo, "rev-parse", "--verify", ref)
        verify_metadata(repo, ref, records)
        local_object = git(repo, "rev-parse", ref).stdout.strip()
        remote_object = queried.get(ref)
        if local_object == remote_object:
            matched_refs += 1
            continue
        if remote_object and not mutable_ref(ref):
            raise ReconstructionError(f"refusing to replace immutable source ref on {args.remote}: {ref}")
        if remote_object:
            leases.append(f"--force-with-lease={ref}:{remote_object}")
        refspecs.append(f"{ref}:{ref}")

    if not refspecs:
        print(f"{args.remote} already matches build and all {matched_refs} selected source ref(s)")
        return

    command = ["git", "push", "--atomic", *leases]
    if not truthy(args.write):
        command.append("--dry-run")
    command.extend([args.remote, *refspecs])
    print("publication command: " + shlex.join(command))
    result = subprocess.run(command, cwd=repo, text=True, check=False)
    if result.returncode != 0:
        raise ReconstructionError(f"atomic source publication failed with exit status {result.returncode}")
    action = "published" if truthy(args.write) else "validated dry-run publication for"
    pending_sources = len(refs) - matched_refs
    targets = f"{pending_sources} source ref(s)"
    if build_pending:
        targets = f"build plus {targets}"
    print(f"{action} {targets} in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
