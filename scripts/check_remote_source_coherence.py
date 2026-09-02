#!/usr/bin/env python3
"""Verify that published source refs match the build-branch manifests."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    git,
    load_ref_records,
    main_wrapper,
    repo_root,
    truthy,
)


HELP = """check-remote-source-coherence

Optional variables:
  REMOTE=<name>  Git remote to inspect. Default: origin.
  WRITE=0|1      With --prepare-local, update runner-local refs. Default: 0.

The check reads the object IDs recorded by config/refs-*.json and compares them
with the corresponding branches and retained Unofficial compatibility tags on
the remote. It does not require the source refs to have been fetched locally
and it does not compare the remote build branch with the checked-out commit.
That makes it suitable for pull requests and local act runs whose candidate
build commit is intentionally ahead of origin/build.

The --prepare-local mode is a CI diagnostic fallback. It updates only local
source refs whose exact manifest-recorded commit objects are already present in
the runner. It never fetches, reconstructs, or substitutes source content, and
it fails if any required object is unavailable. The mode is refused outside CI
or act unless ALLOW_LOCAL_SOURCE_REF_REPAIR=1 is set explicitly.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=HELP,
    )
    p.add_argument("--remote", default=os.environ.get("REMOTE", "origin"))
    p.add_argument("--prepare-local", action="store_true")
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument(
        "--allow-local-source-ref-repair",
        default=os.environ.get("ALLOW_LOCAL_SOURCE_REF_REPAIR", "0"),
    )
    return p


def expected_remote_refs(repo: Path) -> dict[str, str]:
    expected: dict[str, str] = {}
    for record in load_ref_records(repo):
        source_ref = str(record.get("ref", ""))
        object_id = str(record.get("object_id", ""))
        if not source_ref.startswith("source/") or source_ref.startswith(
            ("source/cache/", "source/component/")
        ):
            continue
        if not object_id:
            raise ReconstructionError(
                f"{record.get('manifest', 'config/refs-*.json')}:{source_ref}: "
                "missing object_id"
            )
        branch = f"refs/heads/{source_ref}"
        if branch in expected and expected[branch] != object_id:
            raise ReconstructionError(
                f"conflicting manifest object IDs for {source_ref}: "
                f"{expected[branch]} and {object_id}"
            )
        expected[branch] = object_id

        prefix = "source/unofficial/edk2-stable"
        if source_ref.startswith(prefix):
            release = source_ref.removeprefix(prefix)
            if release:
                expected[f"refs/tags/source/unofficial/edk2/stable-{release}"] = object_id
    return expected


def remote_ref_objects(repo: Path, remote: str) -> dict[str, str]:
    result = git(
        repo,
        "ls-remote",
        remote,
        "refs/heads/source/*",
        "refs/tags/source/*",
        check=False,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise ReconstructionError(
            f"cannot query source refs from {remote}: {detail or 'git ls-remote failed'}"
        )
    objects: dict[str, str] = {}
    peeled: dict[str, str] = {}
    for line in result.stdout.splitlines():
        if not line.strip():
            continue
        object_id, ref = line.split(None, 1)
        if ref.endswith("^{}"):
            peeled[ref[:-3]] = object_id
        else:
            objects[ref] = object_id
    objects.update(peeled)
    return objects


def coherence_errors(expected: dict[str, str], remote: dict[str, str]) -> list[str]:
    errors = []
    for ref, expected_object in sorted(expected.items()):
        actual = remote.get(ref)
        if actual is None:
            errors.append(f"{ref}: missing (expected {expected_object})")
        elif actual != expected_object:
            errors.append(f"{ref}: remote {actual}, manifest {expected_object}")
    return errors


def prepare_local_refs(
    repo: Path,
    expected: dict[str, str],
    *,
    write: bool,
) -> tuple[list[str], list[str]]:
    updates: list[tuple[str, str, str]] = []
    missing: list[str] = []
    zero_oid = "0" * 40
    for ref, expected_object in sorted(expected.items()):
        current_commit = git(
            repo,
            "rev-parse",
            "--verify",
            "--quiet",
            f"{ref}^{{commit}}",
            check=False,
        )
        if current_commit.returncode == 0 and current_commit.stdout.strip() == expected_object:
            continue
        if git(repo, "cat-file", "-e", f"{expected_object}^{{commit}}", check=False).returncode != 0:
            missing.append(f"{ref}: object {expected_object} is unavailable")
            continue
        current_object = git(
            repo,
            "rev-parse",
            "--verify",
            "--quiet",
            ref,
            check=False,
        )
        updates.append(
            (
                ref,
                expected_object,
                current_object.stdout.strip() if current_object.returncode == 0 else zero_oid,
            )
        )

    if write:
        for ref, expected_object, old_object in updates:
            git(repo, "update-ref", ref, expected_object, old_object)
    return [ref for ref, _expected, _old in updates], missing


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    expected = expected_remote_refs(repo)
    if args.prepare_local:
        if not any(
            truthy(value)
            for value in (
                os.environ.get("CI", "0"),
                os.environ.get("ACT", "0"),
                args.allow_local_source_ref_repair,
            )
        ):
            raise ReconstructionError(
                "--prepare-local is restricted to CI/act runners; set "
                "ALLOW_LOCAL_SOURCE_REF_REPAIR=1 only for an intentionally disposable clone"
            )
        updates, missing = prepare_local_refs(repo, expected, write=truthy(args.write))
        action = "updated" if truthy(args.write) else "would update"
        print(f"{action} {len(updates)} runner-local source ref(s) from exact manifest objects")
        if missing:
            detail = "\n".join(f"  - {item}" for item in missing)
            raise ReconstructionError(
                f"cannot prepare {len(missing)} runner-local source ref(s):\n{detail}\n"
                "The missing commits were not uploaded; publish the source update atomically."
            )
        return
    errors = coherence_errors(expected, remote_ref_objects(repo, args.remote))
    if errors:
        detail = "\n".join(f"  - {error}" for error in errors)
        raise ReconstructionError(
            f"{args.remote} source refs do not match the checked-out build metadata "
            f"({len(errors)} mismatch(es)):\n{detail}\n"
            "Publish the source-model update with `make publish-source-update WRITE=1`; "
            "the command infers the required refs automatically."
        )
    print(
        f"{args.remote} matches all {len(expected)} required published source refs"
    )


if __name__ == "__main__":
    main_wrapper(main)
