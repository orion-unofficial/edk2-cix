#!/usr/bin/env python3
"""Check that safe entry points produce prompt first output."""

from __future__ import annotations

import argparse
import os
import re
import selectors
import shutil
import signal
import stat
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from reconstruction_common import ReconstructionError, main_wrapper, repo_root, truthy
from test_import_changes import make_materialised_topic, make_repo
from test_support import commit_all, git, write_file


DEFAULT_THRESHOLD_SECONDS = 0.5
COMMAND_TIMEOUT_SECONDS = 6.0


@dataclass(frozen=True)
class Case:
    name: str
    cmd: tuple[str, ...]
    cwd: Path
    cleanup: Callable[[], None] | None = None


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--threshold", type=float, default=DEFAULT_THRESHOLD_SECONDS)
    p.add_argument("--case", dest="cases", action="append", default=[])
    p.add_argument("--v", default=os.environ.get("V", "0"))
    return p


def make_case(repo: Path, target: str, *args: str) -> Case:
    return Case(
        f"make {target}",
        ("make", "--no-print-directory", target, "FIRST_OUTPUT_PROBE=1", *args),
        repo,
    )


def make_help_case(repo: Path, target: str) -> Case:
    return Case(f"make {target}", ("make", "--no-print-directory", target), repo)


def script_help_case(repo: Path, script: str) -> Case:
    return Case(f"{script} --help", ("python3", script, "--help"), repo)


def copy_makefile(repo: Path, fixture: Path) -> None:
    shutil.copy2(repo / "Makefile", fixture / "Makefile")
    git(fixture, "add", "Makefile")
    git(fixture, "commit", "-m", "add Makefile")


def fixture_import_changes_case(repo: Path) -> Case:
    fixture = make_repo()
    copy_makefile(repo, fixture)
    topic = make_materialised_topic(fixture)
    return Case(
        "make import-changes dry run",
        ("make", "--no-print-directory", "import-changes", f"FROM_REF={topic}"),
        fixture,
        cleanup=lambda: shutil.rmtree(fixture, ignore_errors=True),
    )


def fixture_import_unofficial_case(repo: Path) -> Case:
    fixture = make_repo()
    copy_makefile(repo, fixture)
    git(fixture, "switch", "-c", "unofficial-topic", "source/unofficial/current")
    write_file(fixture, "firmware.txt", "updated by topic\n")
    commit_all(fixture, "topic change")
    git(fixture, "switch", "build")
    return Case(
        "make import-unofficial-commits dry run",
        ("make", "--no-print-directory", "import-unofficial-commits", "FROM_REF=unofficial-topic"),
        fixture,
        cleanup=lambda: shutil.rmtree(fixture, ignore_errors=True),
    )


def phony_targets(repo: Path) -> set[str]:
    makefile = (repo / "Makefile").read_text(encoding="utf-8")
    targets: set[str] = set()
    pattern = re.compile(r"^\\.PHONY:\\s*(.*(?:\\\\\n[^\n]*)*)", re.MULTILINE)
    for match in pattern.finditer(makefile):
        block = match.group(1).replace("\\\n", " ")
        targets.update(part for part in block.split() if part)
    return targets


def executable_scripts(repo: Path) -> set[str]:
    scripts: set[str] = set()
    for root in ("scripts", "docs/scripts"):
        for path in (repo / root).glob("*"):
            if not path.is_file():
                continue
            if path.stat().st_mode & stat.S_IXUSR:
                scripts.add(path.relative_to(repo).as_posix())
    return scripts


def cases(repo: Path) -> list[Case]:
    release = "RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial-1.2.1"
    workflow = "ACT_WORKFLOW=.github/workflows/build-docs.yaml"
    with_dir = f"DIR={Path(tempfile.gettempdir()) / 'edk2-cix-first-output-probe'}"

    make_targets = [
        make_help_case(repo, "help"),
        make_help_case(repo, "help-vars"),
        make_help_case(repo, "help-dev"),
        make_help_case(repo, "help-source-targets"),
        make_case(repo, "build", release),
        make_case(repo, "build-all", release),
        make_case(repo, "install", release),
        make_case(repo, "zip", release),
        make_case(repo, "targz", release),
        make_case(repo, "clean"),
        make_case(repo, "realclean"),
        make_case(repo, "prune"),
        make_case(repo, "create-minimised-clone", with_dir),
        make_case(repo, "buildbox-firmware-build", release),
        make_case(repo, "buildbox-firmware-stage", release),
        make_case(repo, "docs-build"),
        make_case(repo, "docs-workflow-local"),
        make_case(repo, "render-release-branch", release),
        make_case(repo, "verify-release-branch", release),
        make_case(repo, "verify-build-matrix"),
        make_case(repo, "verify-manifest-integrity"),
        make_case(repo, "verify-source-policy"),
        make_case(repo, "verify-source-lifecycle"),
        make_case(repo, "check-ref-integrity"),
        make_case(repo, "verify-minimised-clone", with_dir),
        make_case(repo, "extract-vendor-delta"),
        make_case(repo, "integrate-source-release"),
        make_case(repo, "check-identity-integrity"),
        make_case(repo, "verify-identity-integrity"),
        make_case(repo, "check-vendor-workflow-drift"),
        make_case(repo, "check-upstream-versions"),
        make_case(repo, "check-help-cache"),
        make_case(repo, "check-first-output-latency"),
        make_case(repo, "refresh-help-cache"),
        make_case(repo, "ref-report"),
        make_case(repo, "cleanup-report"),
        make_case(repo, "test"),
        make_case(repo, "lint"),
        make_case(repo, "gha-act-list"),
        make_case(repo, "gha-act-dry-run", workflow),
        make_case(repo, "gha-act-run", workflow),
        make_help_case(repo, "render-release-branch-help"),
        make_help_case(repo, "verify-release-branch-help"),
        make_help_case(repo, "verify-build-matrix-help"),
        make_help_case(repo, "extract-vendor-delta-help"),
        make_help_case(repo, "integrate-source-release-help"),
        make_help_case(repo, "import-unofficial-commits-help"),
        make_help_case(repo, "import-changes-help"),
        make_help_case(repo, "check-vendor-workflow-drift-help"),
        make_help_case(repo, "check-upstream-versions-help"),
        fixture_import_changes_case(repo),
        fixture_import_unofficial_case(repo),
    ]

    script_help = [
        "scripts/check_first_output_latency.py",
        "scripts/check_identity_integrity.py",
        "scripts/check_ref_integrity.py",
        "scripts/check_upstream_versions.py",
        "scripts/check_vendor_workflow_drift.py",
        "scripts/clean_cache.py",
        "scripts/create_minimised_clone.py",
        "scripts/extract_vendor_delta.py",
        "scripts/help_cache.py",
        "scripts/import_changes.py",
        "scripts/import_unofficial_commits.py",
        "scripts/install_release_payload.py",
        "scripts/integrate_source_release.py",
        "scripts/list_source_targets.py",
        "scripts/prepare_release_worktree.py",
        "scripts/prune_cache_refs.py",
        "scripts/quality_checks.py",
        "scripts/ref_report.py",
        "scripts/render_release_branch.py",
        "scripts/validate_build_variables.py",
        "scripts/verify_build_matrix.py",
        "scripts/verify_manifest_integrity.py",
        "scripts/verify_minimised_clone.py",
        "scripts/verify_release_branch.py",
        "scripts/verify_source_lifecycle.py",
        "scripts/verify_source_policy.py",
        "docs/scripts/mdbook-admonish.py",
        "docs/scripts/validate_docs_artifacts.py",
    ]
    return [*make_targets, *(script_help_case(repo, script) for script in script_help)]


SKIPPED_PHONY_TARGETS = {
    "check-first-output-latency",
    "import-changes",
    "import-unofficial-commits",
}


SKIPPED_EXECUTABLE_SCRIPTS = {
    "docs/scripts/install_mdbook_toc.sh",
    "docs/scripts/mdbook-toc.sh",
    "docs/scripts/resolve_temp_root.sh",
    "docs/scripts/run_docs_build.sh",
    "docs/scripts/run_docs_workflow_local.sh",
    "scripts/ensure_act.sh",
    "scripts/run_github_actions_with_act.sh",
    "scripts/run_quality_container.sh",
}


def assert_coverage(repo: Path, case_names: set[str]) -> None:
    covered_targets = {
        name.removeprefix("make ")
        for name in case_names
        if name.startswith("make ") and " dry run" not in name
    }
    covered_targets.update({"import-changes", "import-unofficial-commits"})
    missing_targets = sorted(phony_targets(repo) - covered_targets - SKIPPED_PHONY_TARGETS)
    if missing_targets:
        raise ReconstructionError("first-output latency check has no case for Make target(s): " + ", ".join(missing_targets))

    covered_scripts = {
        name.removesuffix(" --help")
        for name in case_names
        if name.endswith(" --help") and name.startswith(("scripts/", "docs/scripts/"))
    }
    missing_scripts = sorted(executable_scripts(repo) - covered_scripts - SKIPPED_EXECUTABLE_SCRIPTS)
    if missing_scripts:
        raise ReconstructionError("first-output latency check has no case for script(s): " + ", ".join(missing_scripts))


def stop_process(proc: subprocess.Popen[bytes]) -> None:
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    except OSError:
        proc.terminate()
    try:
        proc.wait(timeout=0.5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            proc.kill()
        proc.wait(timeout=1)


def first_output_latency(case: Case, threshold: float) -> float:
    env = os.environ.copy()
    env.update(
        {
            "PYTHONUNBUFFERED": "1",
            "V": "0",
            "DEBUG": "0",
        }
    )
    started = time.monotonic()
    proc = subprocess.Popen(
        case.cmd,
        cwd=case.cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        start_new_session=True,
    )
    assert proc.stdout is not None
    fd = proc.stdout.fileno()
    os.set_blocking(fd, False)
    selector = selectors.DefaultSelector()
    selector.register(fd, selectors.EVENT_READ)
    deadline = started + threshold
    hard_deadline = started + COMMAND_TIMEOUT_SECONDS
    output = bytearray()
    try:
        while True:
            now = time.monotonic()
            if now >= deadline:
                raise AssertionError(f"{case.name} produced no output within {threshold:.3f}s")
            if now >= hard_deadline:
                raise AssertionError(f"{case.name} exceeded hard timeout before first output")
            if proc.poll() is not None:
                try:
                    data = os.read(fd, 4096)
                except BlockingIOError:
                    data = b""
                if data:
                    output.extend(data)
                    if b"\n" in output:
                        return time.monotonic() - started
                if output:
                    raise AssertionError(f"{case.name} exited before producing a complete output line")
                raise AssertionError(f"{case.name} exited without output")
            timeout = max(0.0, min(deadline, hard_deadline) - now)
            events = selector.select(timeout)
            if not events:
                continue
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
            if data:
                output.extend(data)
                if b"\n" in output:
                    return time.monotonic() - started
    finally:
        selector.unregister(fd)
        stop_process(proc)
        if case.cleanup:
            case.cleanup()


def main() -> None:
    args = parser().parse_args()
    repo = repo_root(Path(__file__))
    selected = set(args.cases)
    all_cases = cases(repo)
    assert_coverage(repo, {case.name for case in all_cases})
    if selected:
        by_name = {case.name: case for case in all_cases}
        unknown = selected - set(by_name)
        if unknown:
            raise ReconstructionError("unknown first-output latency case(s): " + ", ".join(sorted(unknown)))
        all_cases = [by_name[name] for name in sorted(selected)]

    failures: list[str] = []
    timings: list[tuple[str, float]] = []
    for case in all_cases:
        try:
            latency = first_output_latency(case, args.threshold)
            timings.append((case.name, latency))
        except Exception as exc:
            failures.append(f"{case.name}: {exc}")

    if truthy(args.v):
        for name, latency in sorted(timings, key=lambda item: item[1], reverse=True):
            print(f"{latency:.3f}s  {name}")

    if failures:
        lines = [f"first-output latency exceeded {args.threshold:.3f}s or failed:"]
        lines.extend(f"  - {failure}" for failure in failures)
        raise ReconstructionError("\n".join(lines))

    print(f"first-output latency check passed for {len(timings)} case(s)")


if __name__ == "__main__":
    main_wrapper(main)
