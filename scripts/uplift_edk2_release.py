#!/usr/bin/env python3
"""Run the mechanical stages of an EDK2 stable-release uplift."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

from reconstruction_common import (
    ReconstructionError,
    clear_metadata_caches,
    format_duration,
    main_wrapper,
    ref_exists,
    release_for_edk2_ref,
    repo_root,
    selected_unofficial_current_ref,
    selected_unofficial_line_policy,
    truthy,
    unofficial_source_policy,
)


HELP = """uplift-edk2-release

Runs the mechanical stages for a new upstream EDK2 stable release:

  1. record source/base/edk2/<EDK2_BASE>
  2. record source/base/edk2-platforms/<EDK2_BASE>
  3. record source/base/edk2-non-osi/<EDK2_BASE>
  4. port the selected Radxa release to the new EDK2 base
  5. promote the selected Unofficial line onto the new Radxa port
  6. record source/unofficial/<RADXA_RELEASE>/<EDK2_BASE>
  7. move source/unofficial/<LINE>/current
  8. render the default custom source target for the new release
  9. run verify-build-matrix

Required variables:
  EDK2_BASE=<edk2-stableYYYYMM>
      New upstream EDK2 stable release.

Optional variables:
  FROM_EDK2_BASE=<edk2-stableYYYYMM>
      Previous EDK2 base used to compute Radxa/unofficial deltas.
      Default: config/policies.json unofficial_source_policy current_edk2_release.
  RADXA_RELEASE=<release>
      Radxa source release to carry forward.
      Default: config/policies.json unofficial_source_policy current_radxa_release.
  LINE=<major.minor>
      Unofficial development line to rebase. Default: the policy-selected line.
  CIX_RELEASE=<release>
      CIX release component set used by the rendered source target.
      Default: config/policies.json unofficial_source_policy current_cix_release.
  EDK2_REF=<ref>
  EDK2_PLATFORMS_REF=<ref>
  EDK2_NON_OSI_REF=<ref>
      Explicit upstream objects to record. Companion refs may usually be omitted;
      integrate-source-release selects the latest upstream master commit at or
      before the EDK2 stable tag timestamp.
  RADXA_REF=<ref>
      Resolved Radxa source-port commit from a conflict handoff.
  UNOFFICIAL_REF=<ref>
      Resolved unofficial source-port commit from a conflict handoff.
  FROM_REF=<ref>
      Source tree to promote for the unofficial stage.
      Default: the policy-selected source/unofficial/<LINE>/current ref.
  RELEASE=<source-target>
      Rendered source target to refresh. Default is derived from EDK2_BASE,
      CIX_RELEASE, and RADXA_RELEASE.
  SKIP_RENDER=0|1
      Skip render-release-branch.
      Default: 0.
  VERIFY=0|1
      Run verify-build-matrix after the render stage.
      Default: 1.
  WRITE=0|1
      Required before refs, tags, or config files are updated. With WRITE=0 the
      command validates inputs, reports skipped/completed stages, and delegates
      dry runs for stages whose prerequisites already exist.
  FORCE=0|1
      Passed to render-release-branch when refreshing an existing cache branch.
      Default: 1 for this orchestrated uplift.
  ALLOW_REPLACE=0|1
      Permit primitive integration commands to replace existing refs instead of
      skipping completed stages.
  V=0|1
      Print delegated commands and git operations.

If a source-port conflict is genuine, the primitive command preserves a
worktree and stops. Resolve the worktree, commit the result there, and rerun
this target with RADXA_REF=<commit> or UNOFFICIAL_REF=<commit> depending on
which stage stopped.
"""


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter, epilog=HELP)
    p.add_argument("--edk2-base", default=os.environ.get("EDK2_BASE", ""))
    p.add_argument("--from-edk2-base", default=os.environ.get("FROM_EDK2_BASE", ""))
    p.add_argument("--radxa-release", default=os.environ.get("RADXA_RELEASE", ""))
    p.add_argument("--line", default=os.environ.get("LINE", ""))
    p.add_argument("--cix-release", default=os.environ.get("CIX_RELEASE", ""))
    p.add_argument("--edk2-ref", default=os.environ.get("EDK2_REF", ""))
    p.add_argument("--edk2-platforms-ref", default=os.environ.get("EDK2_PLATFORMS_REF", ""))
    p.add_argument("--edk2-non-osi-ref", default=os.environ.get("EDK2_NON_OSI_REF", ""))
    p.add_argument("--radxa-ref", default=os.environ.get("RADXA_REF", ""))
    p.add_argument("--unofficial-ref", default=os.environ.get("UNOFFICIAL_REF", ""))
    p.add_argument("--from-ref", default=os.environ.get("FROM_REF", ""))
    p.add_argument("--release", default=os.environ.get("RELEASE", ""))
    p.add_argument("--skip-render", default=os.environ.get("SKIP_RENDER", "0"))
    p.add_argument("--verify", default=os.environ.get("VERIFY", "1"))
    p.add_argument("--write", default=os.environ.get("WRITE", "0"))
    p.add_argument("--force", default=os.environ.get("FORCE", "1"))
    p.add_argument("--allow-replace", default=os.environ.get("ALLOW_REPLACE", "0"))
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def normalise_edk2_base(value: str) -> str:
    value = value.strip()
    if not value:
        return value
    if value.startswith("edk2-stable"):
        return value
    if value.startswith("edk2-"):
        return "edk2-stable" + value.removeprefix("edk2-")
    return "edk2-stable" + value


def policy_defaults(repo: Path) -> dict[str, str]:
    policy = unofficial_source_policy(repo)
    line, selected = selected_unofficial_line_policy(policy)
    defaults = {
        key: str(value).strip()
        for key, value in selected.items()
        if value is not None
    }
    defaults["line"] = line
    return defaults


def source_target(edk2_base: str, cix_release: str, radxa_release: str) -> str:
    release = release_for_edk2_ref(edk2_base)
    if cix_release:
        return f"edk2-{release}/cix-{cix_release}/radxa-{radxa_release}/unofficial"
    return f"edk2-{release}/radxa-{radxa_release}/unofficial"


def run_script(repo: Path, script: str, env_updates: dict[str, str], *, verbose: bool) -> None:
    env = os.environ.copy()
    env.update(env_updates)
    env["PYTHONPATH"] = str(repo / "scripts")
    cmd = [sys.executable, str(repo / "scripts" / script), "--v", env.get("V", "0")]
    if verbose:
        pretty = " ".join(f"{key}={value}" for key, value in sorted(env_updates.items()) if value)
        print(f"[uplift] {pretty} {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=repo, env=env, check=False)
    if result.returncode != 0:
        raise SystemExit(result.returncode)
    clear_metadata_caches()


def run_make(repo: Path, target: str, env_updates: dict[str, str], *, verbose: bool) -> None:
    env = os.environ.copy()
    env.update(env_updates)
    cmd = ["make", "--no-print-directory", target]
    if verbose:
        pretty = " ".join(f"{key}={value}" for key, value in sorted(env_updates.items()) if value)
        print(f"[uplift] {pretty} {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=repo, env=env, check=False)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def maybe_integrate_upstream(
    repo: Path,
    *,
    component: str,
    edk2_base: str,
    explicit_ref: str,
    write: str,
    allow_replace: str,
    verbose: bool,
) -> None:
    target = f"source/base/{component}/{edk2_base}"
    if component == "edk2":
        target = f"source/base/edk2/{edk2_base}"
    if ref_exists(repo, target) and not truthy(allow_replace):
        print(f"[uplift] {target} already exists; skipping")
        return
    print(f"[uplift] integrating {target}")
    run_script(
        repo,
        "integrate_source_release.py",
        {
            "TYPE": "upstream",
            "COMPONENT": component,
            "RELEASE": edk2_base,
            "REF": explicit_ref,
            "WRITE": write,
            "ALLOW_REPLACE": allow_replace,
            "V": "1" if verbose else "0",
        },
        verbose=verbose,
    )


def maybe_integrate_radxa(
    repo: Path,
    *,
    edk2_base: str,
    from_edk2_base: str,
    radxa_release: str,
    radxa_ref: str,
    write: str,
    allow_replace: str,
    verbose: bool,
) -> None:
    target = f"source/port/radxa/{radxa_release}/{edk2_base}"
    if ref_exists(repo, target) and not truthy(allow_replace):
        print(f"[uplift] {target} already exists; skipping")
        return
    if write == "0" and not ref_exists(repo, f"source/cache/base/edk2/{edk2_base}"):
        print(f"[uplift] dry run; would port Radxa {radxa_release} after {edk2_base} base refs are recorded")
        return
    print(f"[uplift] porting Radxa {radxa_release} to {edk2_base}")
    run_script(
        repo,
        "integrate_source_release.py",
        {
            "TYPE": "vendor",
            "VENDOR": "radxa",
            "RELEASE": radxa_release,
            "EDK2_BASE": edk2_base,
            "FROM_EDK2_BASE": from_edk2_base,
            "RADXA_SOURCE": "port",
            "REF": radxa_ref,
            "MATERIALISE": "0" if radxa_ref else "1",
            "WRITE": write,
            "ALLOW_REPLACE": allow_replace,
            "V": "1" if verbose else "0",
        },
        verbose=verbose,
    )


def maybe_promote_unofficial(
    repo: Path,
    *,
    edk2_base: str,
    from_edk2_base: str,
    radxa_release: str,
    line: str,
    cix_release: str,
    from_ref: str,
    unofficial_ref: str,
    write: str,
    allow_replace: str,
    verbose: bool,
) -> None:
    target = f"source/unofficial/{radxa_release}/{edk2_base}"
    if ref_exists(repo, target) and not truthy(allow_replace):
        print(f"[uplift] {target} already exists; skipping")
        return
    if write == "0" and not ref_exists(repo, f"source/cache/base/edk2/{edk2_base}"):
        print(f"[uplift] dry run; would promote unofficial source after {edk2_base} base refs are recorded")
        return
    print(f"[uplift] promoting unofficial source to {edk2_base}")
    run_script(
        repo,
        "promote_unofficial_release.py",
        {
            "EDK2_BASE": edk2_base,
            "FROM_EDK2_BASE": from_edk2_base,
            "RADXA_RELEASE": radxa_release,
            "LINE": line,
            "CIX_RELEASE": cix_release,
            "FROM_REF": from_ref,
            "RESOLVED_REF": unofficial_ref,
            "WRITE": write,
            "ALLOW_REPLACE": allow_replace,
            "UPDATE_CURRENT": "1",
            "UPDATE_POLICY": "1",
            "V": "1" if verbose else "0",
        },
        verbose=verbose,
    )


def main() -> None:
    started = time.monotonic()
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    defaults = policy_defaults(repo)
    verbose = truthy(args.v)
    write = "1" if truthy(args.write) else "0"
    allow_replace = "1" if truthy(args.allow_replace) else "0"

    edk2_base = normalise_edk2_base(args.edk2_base)
    if not edk2_base:
        print(HELP)
        raise SystemExit("missing required variable: EDK2_BASE=<edk2-stableYYYYMM>")

    from_edk2_base = normalise_edk2_base(args.from_edk2_base)
    if not from_edk2_base:
        current = defaults.get("current_edk2_release", "")
        if current:
            from_edk2_base = normalise_edk2_base(current)
    if not from_edk2_base:
        raise ReconstructionError("FROM_EDK2_BASE is required when config/policies.json has no current EDK2 release")
    if release_for_edk2_ref(from_edk2_base) == release_for_edk2_ref(edk2_base):
        raise ReconstructionError(
            "FROM_EDK2_BASE resolves to the same release as EDK2_BASE. "
            "If this is a rerun after policy moved, pass the previous release explicitly."
        )

    radxa_release = args.radxa_release or defaults.get("current_radxa_release", "")
    if not radxa_release:
        raise ReconstructionError("RADXA_RELEASE is required when config/policies.json has no current Radxa release")
    cix_release = args.cix_release or defaults.get("current_cix_release", "")
    line = args.line or defaults.get("line", "")
    if not line:
        raise ReconstructionError("LINE is required when config/policies.json has no selected Unofficial line")
    args.from_ref = args.from_ref or selected_unofficial_current_ref(repo)
    render_target = args.release or source_target(edk2_base, cix_release, radxa_release)

    print(f"[uplift] EDK2 base: {edk2_base}")
    print(f"[uplift] Previous EDK2 base: {from_edk2_base}")
    print(f"[uplift] Radxa release: {radxa_release}")
    print(f"[uplift] Unofficial line: {line}")
    if cix_release:
        print(f"[uplift] CIX release: {cix_release}")
    print(f"[uplift] Render target: {render_target}")

    maybe_integrate_upstream(
        repo,
        component="edk2",
        edk2_base=edk2_base,
        explicit_ref=args.edk2_ref,
        write=write,
        allow_replace=allow_replace,
        verbose=verbose,
    )
    maybe_integrate_upstream(
        repo,
        component="edk2-platforms",
        edk2_base=edk2_base,
        explicit_ref=args.edk2_platforms_ref,
        write=write,
        allow_replace=allow_replace,
        verbose=verbose,
    )
    maybe_integrate_upstream(
        repo,
        component="edk2-non-osi",
        edk2_base=edk2_base,
        explicit_ref=args.edk2_non_osi_ref,
        write=write,
        allow_replace=allow_replace,
        verbose=verbose,
    )
    maybe_integrate_radxa(
        repo,
        edk2_base=edk2_base,
        from_edk2_base=from_edk2_base,
        radxa_release=radxa_release,
        radxa_ref=args.radxa_ref,
        write=write,
        allow_replace=allow_replace,
        verbose=verbose,
    )
    maybe_promote_unofficial(
        repo,
        edk2_base=edk2_base,
        from_edk2_base=from_edk2_base,
        radxa_release=radxa_release,
        line=line,
        cix_release=cix_release,
        from_ref=args.from_ref,
        unofficial_ref=args.unofficial_ref,
        write=write,
        allow_replace=allow_replace,
        verbose=verbose,
    )

    if truthy(args.skip_render):
        print("[uplift] SKIP_RENDER=1; not refreshing rendered source target")
    elif write == "0":
        print("[uplift] dry run; would render source target after WRITE=1")
    else:
        print(f"[uplift] rendering {render_target}")
        run_script(
            repo,
            "render_release_branch.py",
            {
                "RELEASE": render_target,
                "PERSIST": "1",
                "REBUILD": "1",
                "FORCE": "1" if truthy(args.force) else "0",
                "V": "1" if verbose else "0",
            },
            verbose=verbose,
        )

    if truthy(args.verify) and write == "1":
        print("[uplift] verifying build matrix")
        run_make(repo, "verify-build-matrix", {"V": "1" if verbose else "0"}, verbose=verbose)
    elif truthy(args.verify):
        print("[uplift] dry run; would run verify-build-matrix after WRITE=1")

    print(f"EDK2 uplift orchestration completed in {format_duration(time.monotonic() - started)}")


if __name__ == "__main__":
    main_wrapper(main)
