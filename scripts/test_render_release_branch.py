#!/usr/bin/env python3
"""Regression tests for release-worktree cache handling."""

from __future__ import annotations

import os
import json
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager, redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch

from test_support import commit_all, git, load_function_tests, require, write_file
from reconstruction_common import ReconstructionError, rendered_release_type


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from render_release_branch import (  # noqa: E402
    apply_release_metadata,
    cached_worktree_is_dirty,
    ensure_worktree,
    render_from_plan,
    validate_release_metadata,
)


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-render-release-test."))
    git(repo, "init", "-b", "build")
    git(repo, "config", "user.name", "Render Release Test")
    git(repo, "config", "user.email", "render-release-test")
    write_file(repo, ".gitattributes", "*.txt text eol=crlf\n")
    commit_all(repo, "configure line endings")
    return repo


CUSTOM_TARGET = "source/cache/release/custom/edk2-202208/radxa-1.3.1/unofficial-1.3.1"
UPSTREAM_TARGET = "source/cache/release/upstream/edk2-202208/radxa-1.3.1"


@contextmanager
def rendering_fixture():
    """Real renderer/Git inputs, with harmless source Make targets only."""
    repo = make_repo()
    try:
        write_file(repo, ".gitignore", ".cache/\n.worktrees/\n")
        write_file(repo, "VERSION", "1.3.1\n")
        write_file(repo, "debian/changelog", "edk2-cix (1.3.1) main; urgency=medium\n")
        write_file(repo, "src/Makefile", "\t\t\tupstream) \\\n\t\t\t\t;; \\\n")
        write_file(repo, "scripts/ensure_build_deps.sh", "    python3\n")
        write_file(repo, "Makefile", """.PHONY: buildbox-firmware-build build-all buildbox-zip buildbox-targz buildbox-firmware-stage deterministic-replay
buildbox-firmware-build build-all buildbox-zip buildbox-targz buildbox-firmware-stage deterministic-replay:
\t@printf '%s\\n' '$(CURDIR)' '$(MAKECMDGOALS)' '$(V)' '$(SIGNING_CERT_SOURCE_DIR)' > "$(FIXTURE_RESULT)"
\t@printf 'fixture delegate stderr\\n' >&2
\t@exit $(FIXTURE_EXIT)
""")
        source = commit_all(repo, "record harmless firmware delegation fixture")
        refs = ["source/base/edk2/edk2-stable202208",
                "source/vendor/radxa/1.3.1/edk2-stable202208",
                "source/unofficial/1.3.1/edk2-stable202208",
                "source/unofficial/edk2-stable202208"]
        for ref in refs:
            git(repo, "branch", ref, source)
        git(repo, "tag", "source/unofficial/edk2/stable-202208", source)
        tree = git(repo, "rev-parse", source + "^{tree}").stdout.strip()
        write_file(repo, "config/refs-fixture.json", json.dumps({"refs": [
            {"ref": ref, "object_id": source, "tree_id": tree, "immutable": True}
            for ref in refs
        ]}) + "\n")
        # The CLI locates its repository from __file__, so copy the real harness.
        shutil.copytree(ROOT / "scripts", repo / "scripts", dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("__pycache__"))
        shutil.copy2(ROOT / "Makefile", repo / "Makefile")
        commit_all(repo, "record build harness in disposable repository")
        yield repo
    finally:
        shutil.rmtree(repo)


def renderer_command(repo: Path, target: str, verbose: str, *args: str):
    env = os.environ.copy()
    env.update(EDK2_CIX_TMP_ROOT=str(repo / ".worktrees" / "scratch"),
               EDK2_CIX_WORKTREE_NAMESPACE="", PERSIST="0", REBUILD="0", FORCE="0")
    return subprocess.run(
        [sys.executable, "scripts/render_release_branch.py", "--release", target,
         "--v", verbose, *args], cwd=repo, env=env, capture_output=True, text=True,
    )


def fixture_source_state(repo: Path):
    return (git(repo, "for-each-ref", "--format=%(refname) %(objectname)").stdout,
            (repo / "config/refs-fixture.json").read_bytes())


def test_renderer_cli_path_and_ref_stdout_are_exact_at_both_verbosity_levels() -> None:
    for verbose in ("0", "1"):
        for target in (CUSTOM_TARGET, UPSTREAM_TARGET):
            with rendering_fixture() as repo:
                before = fixture_source_state(repo)
                result = renderer_command(repo, target, verbose, "--ensure-worktree", "--print-worktree")
                require(result.returncode == 0, result.stderr)
                path = Path(result.stdout.rstrip("\n"))
                require(result.stdout == str(path) + "\n" and path.is_dir(),
                        f"stdout is not exactly one valid worktree path: {result.stdout!r}")
                require((path / "VERSION").read_text() == "1.3.1\n", "wrong release worktree")
                require("[render] Prepared " + target in result.stderr, "missing renderer diagnostics")
                if verbose == "1":
                    require("rm '" in result.stderr and "render:" in result.stderr,
                            "verbose delegated Git stdout was not retained on stderr")
                else:
                    require("rm '" not in result.stderr, "default mode became verbose")
                ref_result = renderer_command(repo, target, verbose, "--print-ref")
                require(ref_result.returncode == 0, ref_result.stderr)
                require(ref_result.stdout == git(path, "rev-parse", "HEAD").stdout,
                        "--print-ref stdout contains diagnostics or the wrong commit")
                require(fixture_source_state(repo) == before, "renderer moved refs or changed manifests")


def test_renderer_git_failure_keeps_machine_stdout_empty() -> None:
    for verbose in ("0", "1"):
        with rendering_fixture() as repo:
            before = fixture_source_state(repo)
            hook = repo / ".git/hooks/pre-commit"
            hook.write_text("#!/bin/sh\necho hook-stdout\necho hook-stderr >&2\nexit 37\n")
            hook.chmod(0o755)
            result = renderer_command(repo, CUSTOM_TARGET, verbose, "--print-worktree")
            require(result.returncode == 2, f"Git failure was swallowed: {result}")
            require(result.stdout == "", f"failure contaminated stdout: {result.stdout!r}")
            require("hook-stderr" in result.stderr and "command failed: git" in result.stderr,
                    "Git failure diagnostics were lost")
            if verbose == "1":
                require("hook-stdout" in result.stderr, "verbose Git stdout diagnostic was lost")
            require(fixture_source_state(repo) == before, "failed render changed source identity")


def test_renderer_git_preserves_exit_status_and_captured_queries() -> None:
    with rendering_fixture() as repo:
        alias = "alias.output-contract=!printf 'git-stdout\\n'; printf 'git-stderr\\n' >&2; exit 37"
        code = """from pathlib import Path
from render_release_branch import git
import sys
captured = sys.argv[3] == '1'
result = git(Path(sys.argv[1]), '-c', sys.argv[2], 'output-contract', check=False, capture=captured)
if captured:
    assert result.stdout == 'git-stdout\\n' and result.stderr == 'git-stderr\\n'
sys.exit(result.returncode)
"""
        for capture in ("0", "1"):
            result = subprocess.run([sys.executable, "-c", code, str(repo), alias, capture],
                                    env=dict(os.environ, PYTHONPATH=str(ROOT / "scripts")),
                                    capture_output=True, text=True)
            require(result.returncode == 37, f"Git exit status changed: {result}")
            require(result.stdout == "", "uncaptured Git wrote to the machine output stream")
            require(result.stderr == ("git-stdout\ngit-stderr\n" if capture == "0" else ""),
                    "Git diagnostics or captured-query semantics changed")


def test_signing_argument_stdout_is_exact_at_both_verbosity_levels() -> None:
    for verbose in ("0", "1"):
        with tempfile.TemporaryDirectory(prefix="signing-argument-") as tmp:
            root = Path(tmp)
            source = root / "certs"
            source.mkdir()
            for name in ("trusted_key_no.crt", "nt_fw_cert.crt", "nt_fw_key.crt"):
                (source / name).write_text("fixture certificate\n")
            command = [sys.executable, str(ROOT / "scripts/prepare_release_worktree.py"),
                       "--worktree", str(root / "worktree"), "--signing-cert-source-dir", str(source),
                       "--print-make-arg", "--v", verbose]
            result = subprocess.run(command, capture_output=True, text=True)
            require(result.returncode == 0, result.stderr)
            prefix = "SIGNING_CERT_SOURCE_DIR="
            require(result.stdout.startswith(prefix) and result.stdout.count("\n") == 1,
                    "signing argument stdout was contaminated")
            staged = root / "worktree" / result.stdout.removeprefix(prefix).strip()
            require((staged / "nt_fw_cert.crt").read_bytes() == (source / "nt_fw_cert.crt").read_bytes(),
                    "signing certificate staging changed")
            require(bool(result.stderr) == (verbose == "1"), "signing verbosity changed")


def test_cache_ignores_only_raw_byte_identical_normalization_noise() -> None:
    repo = make_repo()
    try:
        path = repo / "legacy.txt"
        path.write_bytes(b"one\r\ntwo\r\n")
        object_id = git(repo, "hash-object", "-w", "--no-filters", "legacy.txt").stdout.strip()
        git(repo, "update-index", "--add", "--cacheinfo", f"100644,{object_id},legacy.txt")
        git(repo, "commit", "-m", "record legacy CRLF blob")

        require(git(repo, "status", "--porcelain").stdout, "fixture should trigger Git normalization noise")
        require(
            not cached_worktree_is_dirty(repo),
            "raw-byte-identical tracked file should not dirty the cache",
        )

        path.write_bytes(b"changed\r\n")
        require(cached_worktree_is_dirty(repo), "real tracked content change was ignored")
    finally:
        shutil.rmtree(repo)


def test_cache_rejects_staged_deleted_and_untracked_changes() -> None:
    repo = make_repo()
    try:
        write_file(repo, "tracked.txt", "tracked\n")
        commit_all(repo, "add tracked file")

        write_file(repo, "untracked.txt", "untracked\n")
        require(cached_worktree_is_dirty(repo), "untracked file was ignored")
        (repo / "untracked.txt").unlink()

        (repo / "tracked.txt").unlink()
        require(cached_worktree_is_dirty(repo), "deleted file was ignored")
        git(repo, "restore", "tracked.txt")

        write_file(repo, "tracked.txt", "staged\n")
        git(repo, "add", "tracked.txt")
        require(cached_worktree_is_dirty(repo), "staged change was ignored")
    finally:
        shutil.rmtree(repo)


def test_unofficial_release_metadata_must_match_selected_radxa_release() -> None:
    repo = make_repo()
    try:
        write_file(repo, "VERSION", "1.3.1\n")
        write_file(repo, "debian/changelog", "edk2-cix (1.3.1) main; urgency=medium\n")
        matching = commit_all(repo, "matching metadata")
        entry = {"unofficial_delta": True, "radxa_release": "1.3.1"}
        validate_release_metadata(repo, matching, entry, "custom-target")

        write_file(repo, "VERSION", "1.2.1\n")
        stale = commit_all(repo, "stale VERSION")
        try:
            validate_release_metadata(repo, stale, entry, "custom-target")
        except ReconstructionError as exc:
            require("VERSION=1.2.1" in str(exc), "metadata error omitted the stale VERSION")
        else:
            raise AssertionError("stale unofficial release metadata was accepted")
    finally:
        shutil.rmtree(repo)


def test_release_metadata_is_aligned_from_radxa_source() -> None:
    repo = make_repo()
    worktree = None
    try:
        write_file(repo, "VERSION", "1.2.1\n")
        write_file(repo, "debian/changelog", "edk2-cix (1.2.1) main; urgency=medium\n")
        stale = commit_all(repo, "stale metadata")
        write_file(repo, "debian/changelog", "edk2-cix (1.3.1) main; urgency=medium\n\n  * vendor release\n")
        vendor = commit_all(repo, "Radxa metadata")
        worktree = repo.parent / f"{repo.name}-worktree"
        git(repo, "worktree", "add", "--detach", str(worktree), stale)

        apply_release_metadata(repo, worktree, vendor, "1.3.1", verbose=False)
        require((worktree / "VERSION").read_text(encoding="utf-8") == "1.3.1\n", "VERSION was not aligned")
        require(
            (worktree / "debian/changelog").read_text(encoding="utf-8")
            == "edk2-cix (1.3.1) main; urgency=medium\n\n  * vendor release\n",
            "Radxa changelog was not preserved",
        )
    finally:
        if worktree is not None:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False)
        shutil.rmtree(repo)


def test_synthetic_render_commit_does_not_require_operator_identity() -> None:
    repo = make_repo()
    try:
        base = git(repo, "rev-parse", "HEAD").stdout.strip()
        git(repo, "config", "--unset-all", "user.name")
        git(repo, "config", "--unset-all", "user.email")
        rendered = render_from_plan(
            repo,
            "source/cache/release/custom/test",
            {"render": {"base": {"ref": base}, "steps": [], "remove_root_gitmodules": False}},
            verbose=False,
        )
        require(
            git(repo, "show", "-s", "--format=%an <%ae>", rendered).stdout.strip()
            == "EDK2 CIX renderer <edk2-cix-renderer>",
            "synthetic render commit inherited an operator identity",
        )
    finally:
        shutil.rmtree(repo)


def test_verbose_worktree_creation_keeps_stdout_machine_readable() -> None:
    repo = make_repo()
    worktree = None
    try:
        output = StringIO()
        diagnostics = StringIO()
        with redirect_stdout(output), redirect_stderr(diagnostics):
            worktree = ensure_worktree(repo, "test-release", "HEAD", verbose=True)
        require(output.getvalue() == "", "verbose git output leaked onto stdout")
        require(
            "Creating detached release worktree" in diagnostics.getvalue(),
            "verbose worktree diagnostics were lost",
        )
    finally:
        if worktree is not None:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False)
        shutil.rmtree(repo)


def test_worktree_namespace_isolates_parallel_ci_jobs() -> None:
    repo = make_repo()
    worktree = None
    try:
        with patch.dict(os.environ, {"EDK2_CIX_WORKTREE_NAMESPACE": "replay/O6"}):
            worktree = ensure_worktree(repo, "test-release", "HEAD", verbose=False)
        require(
            worktree.parent == repo / ".cache" / "edk2-cix" / "worktrees" / "replay_O6",
            "configured worktree namespace was not applied",
        )
    finally:
        if worktree is not None:
            git(repo, "worktree", "remove", "--force", str(worktree), check=False)
        shutil.rmtree(repo)


def test_rendered_release_type_tracks_the_target_stage() -> None:
    require(
        rendered_release_type("source/cache/release/upstream/edk2-202208/radxa-1.3.1")
        == "rendered-upstream-radxa-release",
        "upstream replay target received the generic rendered type",
    )
    require(
        rendered_release_type("source/cache/release/vendor/edk2-202208/cix-1.2/radxa-1.3.1")
        == "rendered-vendor-release",
        "vendor target received the generic rendered type",
    )
    require(
        rendered_release_type(
            "source/cache/release/custom/edk2-202608/cix-1.2/radxa-1.3.1/unofficial"
        )
        == "rendered-release",
        "custom target did not retain the generic rendered type",
    )


def load_tests(_loader, _tests, _pattern):
    return load_function_tests(globals())


def main() -> None:
    test_cache_ignores_only_raw_byte_identical_normalization_noise()
    test_cache_rejects_staged_deleted_and_untracked_changes()
    print("release worktree cache regression tests passed")


if __name__ == "__main__":
    main()
