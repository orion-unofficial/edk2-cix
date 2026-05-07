#!/usr/bin/env python3
"""Validate deterministic source lifecycle projection for unofficial refs."""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    format_duration,
    unofficial_release_branches,
    main_wrapper,
    ref_exists,
    repo_root,
    truthy,
)
from source_lifecycle import (
    format_projection,
    normalisation_blockers,
    project_overlay_tree,
    required_normalisation,
    summarise_projections,
)
from source_policy import enforce_source_tree_policy


HELP = """verify-source-lifecycle

Optional variables:
  FROM_REF=<ref>
      Source ref whose overlay paths should be projected.
      Default: source/unofficial/current.
  TARGET_REF=<ref[,ref...]>
      Specific target refs to validate. If omitted, every
      source/unofficial/edk2-stable* release branch is used.
  V=0|1
      Print per-target projection summaries.
  SOURCE_LIFECYCLE_NORMALISE=off|validate|mirror|exact
      Normalisation mode to validate against the current source refs.
      Default: exact.

The checker uses exact Git object identity for automatic rename handling. It
fails rather than guessing when a source path maps to multiple exact candidates.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", "source/unofficial/current"))
    p.add_argument("--target-ref", default=os.environ.get("TARGET_REF", ""))
    p.add_argument("--normalise-mode", default=os.environ.get("SOURCE_LIFECYCLE_NORMALISE", "exact"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def target_refs(repo: Path, value: str) -> list[str]:
    if value.strip():
        return [item.strip() for item in value.split(",") if item.strip()]
    return unofficial_release_branches(repo)


def require_ref(repo: Path, ref: str, label: str) -> None:
    if not ref_exists(repo, ref):
        raise ReconstructionError(f"{label} is unavailable locally: {ref}")


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    verbose = truthy(args.v)
    started = time.monotonic()

    require_ref(repo, args.from_ref, "FROM_REF")
    targets = target_refs(repo, args.target_ref)
    if not targets:
        raise ReconstructionError("no source lifecycle target refs are available")
    for ref in targets:
        require_ref(repo, ref, "TARGET_REF")

    enforce_source_tree_policy(repo, ref=args.from_ref, label=args.from_ref)

    total = 0
    normalise_total = 0
    problems: list[str] = []
    for target in targets:
        enforce_source_tree_policy(repo, ref=target, label=target)
        projections = project_overlay_tree(repo, args.from_ref, target)
        total += len(projections)
        normalise_required = required_normalisation(projections)
        normalise_total += len(normalise_required)
        blockers = normalisation_blockers(projections, args.normalise_mode)
        if verbose:
            summary = ", ".join(f"{key}={value}" for key, value in summarise_projections(projections).items())
            print(
                f"{target}: {len(projections)} overlay projection(s); "
                f"{len(normalise_required)} normalisation action(s); {summary or 'no overlay paths'}"
            )
        if blockers:
            problems.append(f"{target}:")
            problems.extend(f"  - {format_projection(item)}" for item in blockers[:20])
            if len(blockers) > 20:
                problems.append(f"  ... and {len(blockers) - 20} more")

    if problems:
        raise ReconstructionError(
            f"source lifecycle validation failed for SOURCE_LIFECYCLE_NORMALISE={args.normalise_mode}:\n"
            + "\n".join(problems)
        )

    print(
        f"validated source lifecycle: {len(targets)} target ref(s), "
        f"{total} overlay projection(s), {normalise_total} deterministic normalisation action(s), "
        f"0 errors in {format_duration(time.monotonic() - started)}"
    )


if __name__ == "__main__":
    try:
        main_wrapper(main)
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        raise SystemExit(130)
