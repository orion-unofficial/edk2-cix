#!/usr/bin/env python3
"""Regression tests for deterministic source metadata refreshes."""

from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from test_support import commit_all, git, load_function_tests, load_json, require, rev_parse, run, switch_orphan, write_file


ROOT = Path(__file__).resolve().parent.parent
SCRIPT_FILES = [
    "import_workflow.py",
    "reconstruction_common.py",
    "refresh_source_metadata.py",
    "render_release_branch.py",
    "source_policy.py",
]
ZERO = "0" * 40


def create_branch(repo: Path, ref: str, files: dict[str, str], message: str) -> str:
    switch_orphan(repo, ref)
    for path, content in files.items():
        write_file(repo, path, content)
    return commit_all(repo, message)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-refresh-source-metadata-test."))
    git(repo, "init", "-b", "build")
    git(repo, "config", "user.name", "Refresh Metadata Test")
    git(repo, "config", "user.email", "refresh-metadata-test")
    scripts = repo / "scripts"
    scripts.mkdir()
    for name in SCRIPT_FILES:
        shutil.copy2(ROOT / "scripts" / name, scripts / name)
    commit_all(repo, "build scripts")

    create_branch(repo, "source/base/edk2/edk2-stable202208", {"src/edk2.txt": "edk2\n"}, "edk2 base")
    create_branch(repo, "source/base/edk2-platforms/edk2-stable202208", {"src/platform.txt": "platforms\n"}, "platforms base")
    create_branch(repo, "source/base/edk2-non-osi/edk2-stable202208", {"src/non-osi.txt": "non-osi\n"}, "non-osi base")
    create_branch(repo, "source/vendor/radxa/1.2.1/edk2-stable202208", {
        "src/radxa.txt": "radxa\n",
        "src/cix-v1.2/tf-a/original.txt": "old tf-a\n",
        "src/cix-v1.2/tee/original.txt": "old tee\n",
    }, "radxa source")
    create_branch(repo, "source/vendor/cix/1.2/tf-a", {"tf-a.txt": "new tf-a\n"}, "cix tf-a")
    create_branch(repo, "source/vendor/cix/1.2/op-tee", {"op-tee.txt": "new op-tee\n"}, "cix op-tee")
    create_branch(repo, "source/vendor/cix/1.2/bios", {"bios.txt": "bios\n"}, "cix bios")
    current = create_branch(repo, "source/unofficial/1.2/current", {"src/current.txt": "current\n"}, "current unofficial")
    release = create_branch(repo, "source/unofficial/edk2-stable202208", {
        "scripts/ensure_build_deps.sh": (
            "common_packages=(\n"
            "    python3\n"
            "    python3-cryptography\n"
            ")\n"
        ),
        "src/Makefile": (
            "PATCHED_EDK2_SOURCE_INPUTS := \\\n"
            "\t$(abspath missing-custom-source.c)\n"
            "\t\t\tupstream) \\\n"
            "\t\t\t\t;; \\\n"
        ),
        "src/current.txt": "release\n",
    }, "release unofficial")
    git(repo, "tag", "source/unofficial/edk2/stable-202208", current)

    git(repo, "switch", "build")
    write_config(repo)
    commit_all(repo, "stale config")
    require(release != current, "test fixture tag should initially be stale")
    return repo


def write_config(repo: Path) -> None:
    write_json(repo / "config/refs-edk2.json", {
        "component_templates": {
            "edk2": {"ref": "source/base/edk2/{edk2_ref}", "upstream_ref": "refs/tags/{edk2_ref}"},
            "edk2-non-osi": {"ref": "source/base/edk2-non-osi/{edk2_ref}", "upstream_ref": "refs/heads/master"},
            "edk2-platforms": {"ref": "source/base/edk2-platforms/{edk2_ref}", "upstream_ref": "refs/heads/master"},
        },
        "defaults": {"immutable": True, "type": "base"},
        "releases": [{
            "edk2_ref": "edk2-stable202208",
            "components": {
                "edk2": {"object_id": ZERO, "tree_id": ZERO},
                "edk2-non-osi": {"object_id": ZERO, "tree_id": ZERO},
                "edk2-platforms": {"object_id": ZERO, "tree_id": ZERO},
            },
        }],
    })
    write_json(repo / "config/refs-cix.json", {
        "defaults": {"immutable": True, "vendor": "cix"},
        "refs": [
            {"ref": "source/vendor/cix/1.2/bios", "component": "bios", "object_id": ZERO, "tree_id": ZERO},
            {"ref": "source/vendor/cix/1.2/op-tee", "component": "op-tee", "object_id": ZERO, "tree_id": ZERO},
            {"ref": "source/vendor/cix/1.2/tf-a", "component": "tf-a", "object_id": ZERO, "tree_id": ZERO},
        ],
    })
    write_json(repo / "config/refs-radxa.json", {
        "defaults": {"format": "materialised source tree", "immutable": True, "vendor": "radxa"},
        "refs": [{
            "ref": "source/vendor/radxa/1.2.1/edk2-stable202208",
            "object_id": ZERO,
            "tree_id": ZERO,
        }],
    })
    write_json(repo / "config/refs-unofficial.json", {
        "refs": [{
            "ref": "source/unofficial/edk2-stable202208",
            "object_id": ZERO,
            "tree_id": ZERO,
            "type": "unofficial-release-checkpoint",
        }],
    })
    write_json(repo / "config/refs-source-target-cache.json", {
        "defaults": {"immutable": True, "type": "rendered-release"},
        "refs": [
            {
                "refs": [
                    "source/cache/release/custom/edk2-202208/cix-1.2/radxa-1.2.1/unofficial",
                    "source/cache/release/custom/edk2-202208/radxa-1.2.1/unofficial",
                ],
                "tree_id": ZERO,
            },
            {
                "ref": "source/cache/release/vendor/edk2-202208/cix-1.2/radxa-1.2.1",
                "tree_id": ZERO,
                "type": "rendered-vendor-release",
            },
            {
                "ref": "source/cache/release/upstream/edk2-202208/radxa-1.2.1",
                "tree_id": ZERO,
            },
        ],
    })


def run_refresh(repo: Path, **env: str) -> subprocess.CompletedProcess[str]:
    return run(["python3", "scripts/refresh_source_metadata.py"], repo, check=False, env=env)


def tag_commit(repo: Path) -> str:
    return rev_parse(repo, "refs/tags/source/unofficial/edk2/stable-202208")


def vendor_cache_ref() -> str:
    return "source/cache/release/vendor/edk2-202208/cix-1.2/radxa-1.2.1"


def test_dry_run_does_not_modify_metadata_or_tags() -> None:
    repo = make_repo()
    try:
        before_config = (repo / "config/refs-source-target-cache.json").read_text(encoding="utf-8")
        before_tag = tag_commit(repo)
        result = run_refresh(repo, RENDER_GENERATED="1", UPDATE_RELEASE_TAGS="1")
        require(result.returncode == 0, result.stderr)
        require("[metadata] Rendering generated source target:" in result.stdout, result.stdout)
        require("dry run; set WRITE=1" in result.stdout, result.stdout)
        require((repo / "config/refs-source-target-cache.json").read_text(encoding="utf-8") == before_config, "dry run rewrote config")
        require(tag_commit(repo) == before_tag, "dry run moved release tag")
    finally:
        shutil.rmtree(repo)


def test_full_render_does_not_trust_existing_generated_cache_ref() -> None:
    repo = make_repo()
    try:
        cache_ref = vendor_cache_ref()
        create_branch(repo, cache_ref, {"wrong-cache.txt": "wrong\n"}, "wrong generated cache")
        wrong_cache_tree = git(repo, "rev-parse", f"{cache_ref}^{{tree}}").stdout.strip()
        git(repo, "switch", "build")

        refreshed = run_refresh(repo, WRITE="1", RENDER_GENERATED="1")
        require(refreshed.returncode == 0, refreshed.stderr)

        cache = load_json(repo / "config/refs-source-target-cache.json")
        rendered_tree = cache["refs"][1]["tree_id"]
        require(rendered_tree != wrong_cache_tree, "full render reused an existing generated cache ref")

        current = run_refresh(repo, CHECK="1", RENDER_GENERATED="1")
        require(current.returncode != 0, "stale persistent cache ref unexpectedly passed")
        require("persistent source-target cache refs" in current.stdout, current.stdout)

        rebuilt = run(
            [
                "python3",
                "scripts/render_release_branch.py",
                "--release",
                cache_ref,
                "--persist",
                "1",
                "--rebuild",
                "1",
                "--force",
                "1",
            ],
            repo,
            check=False,
        )
        require(rebuilt.returncode == 0, rebuilt.stderr + rebuilt.stdout)

        current = run_refresh(repo, CHECK="1", RENDER_GENERATED="1")
        require(current.returncode == 0, current.stderr + current.stdout)
    finally:
        shutil.rmtree(repo)


def test_refresh_preserves_inactive_retained_custom_target() -> None:
    repo = make_repo()
    try:
        cix_ref = (
            "source/cache/release/custom/edk2-202208/cix-1.2/"
            "radxa-1.2.1/unofficial"
        )
        plain_ref = (
            "source/cache/release/custom/edk2-202208/"
            "radxa-1.2.1/unofficial"
        )
        retained = create_branch(
            repo,
            cix_ref,
            {"src/current.txt": "retained mutable-tip snapshot\n"},
            "retained mutable-tip snapshot",
        )
        git(repo, "branch", plain_ref, retained)
        git(repo, "switch", "build")

        refreshed = run_refresh(repo, WRITE="1")
        require(refreshed.returncode == 0, refreshed.stderr + refreshed.stdout)
        cache = load_json(repo / "config/refs-source-target-cache.json")
        require(
            cache["refs"][0]["tree_id"]
            == git(repo, "rev-parse", f"{retained}^{{tree}}").stdout.strip(),
            "metadata refresh rebound an inactive retained custom target",
        )
    finally:
        shutil.rmtree(repo)


def test_refresh_repairs_hashes_cache_trees_and_tags() -> None:
    repo = make_repo()
    try:
        stale = run_refresh(repo, CHECK="1", RENDER_GENERATED="1", UPDATE_RELEASE_TAGS="1")
        require(stale.returncode != 0, "stale metadata check unexpectedly passed")
        require("source metadata is stale" in stale.stderr, stale.stderr)

        refreshed = run_refresh(repo, WRITE="1", RENDER_GENERATED="1", UPDATE_RELEASE_TAGS="1")
        require(refreshed.returncode == 0, refreshed.stderr)
        require("wrote refreshed source metadata" in refreshed.stdout, refreshed.stdout)

        current = run_refresh(repo, CHECK="1", RENDER_GENERATED="1", UPDATE_RELEASE_TAGS="1")
        require(current.returncode == 0, current.stderr + current.stdout)

        edk2 = load_json(repo / "config/refs-edk2.json")
        require(edk2["releases"][0]["components"]["edk2"]["object_id"] == rev_parse(repo, "source/base/edk2/edk2-stable202208"), "edk2 object_id was not refreshed")
        unofficial = load_json(repo / "config/refs-unofficial.json")
        require(unofficial["refs"][0]["object_id"] == rev_parse(repo, "source/unofficial/edk2-stable202208"), "unofficial object_id was not refreshed")
        require(unofficial["refs"][0]["tree_id"] == git(repo, "rev-parse", "source/unofficial/edk2-stable202208^{tree}").stdout.strip(), "unofficial tree_id was not refreshed")
        cache = load_json(repo / "config/refs-source-target-cache.json")
        custom_tree = cache["refs"][0]["tree_id"]
        require(custom_tree == git(repo, "rev-parse", "source/unofficial/edk2-stable202208^{tree}").stdout.strip(), "custom cache tree was not derived from source/unofficial")
        vendor = next(
            item for item in cache["refs"]
            if item.get("ref") == "source/cache/release/vendor/edk2-202208/cix-1.2/radxa-1.2.1"
        )
        require(vendor["tree_id"] != ZERO, "rendered vendor cache tree was not regenerated")
        upstream = next(
            item for item in cache["refs"]
            if item.get("ref") == "source/cache/release/upstream/edk2-202208/radxa-1.2.1"
        )
        require(
            upstream.get("type") == "rendered-upstream-radxa-release",
            "upstream cache metadata inherited the generic rendered type",
        )
        require(tag_commit(repo) == rev_parse(repo, "source/unofficial/edk2-stable202208"), "release tag was not updated")
    finally:
        shutil.rmtree(repo)


def main() -> None:
    test_dry_run_does_not_modify_metadata_or_tags()
    test_full_render_does_not_trust_existing_generated_cache_ref()
    test_refresh_preserves_inactive_retained_custom_target()
    test_refresh_repairs_hashes_cache_trees_and_tags()
    print("refresh_source_metadata tests passed")


def load_tests(loader, tests, pattern):
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
