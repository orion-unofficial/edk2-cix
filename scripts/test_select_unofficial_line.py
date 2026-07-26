#!/usr/bin/env python3
"""Regression tests for unofficial default-line selection."""

from __future__ import annotations

import json
import shutil
import sys
import tempfile
from pathlib import Path

from test_support import commit_all, git, load_function_tests, require, switch_orphan, write_file


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from reconstruction_common import ReconstructionError, selected_unofficial_current_ref  # noqa: E402
from select_unofficial_line import select_line  # noqa: E402


def make_repo() -> Path:
    repo = Path(tempfile.mkdtemp(prefix="edk2-cix-select-line-test."))
    git(repo, "init", "-b", "build")
    git(repo, "config", "user.name", "Select Line Test")
    git(repo, "config", "user.email", "select-line-test")
    write_file(
        repo,
        "config/policies.json",
        json.dumps(
            {
                "immutability": {
                    "mutable_unofficial_patterns": ["source/unofficial/*/current"],
                },
                "unofficial_source_policy": {
                    "default_line": "1.2",
                    "lines": {
                        "1.2": {
                            "current_cix_release": "1.2",
                            "current_edk2_release": "202605",
                            "current_radxa_release": "1.2.4",
                            "current_ref": "source/unofficial/1.2/current",
                        },
                        "1.3": {
                            "current_cix_release": "1.2",
                            "current_edk2_release": "202605",
                            "current_radxa_release": "1.3.1",
                            "current_ref": "source/unofficial/1.3/current",
                        },
                    },
                    "prefer_versioned_default_alias": True,
                },
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
    )
    commit_all(repo, "build policy")

    switch_orphan(repo, "source/unofficial/1.2/current")
    write_file(repo, "line.txt", "1.2\n")
    commit_all(repo, "line 1.2")
    switch_orphan(repo, "source/unofficial/1.3/current")
    write_file(repo, "line.txt", "1.3\n")
    commit_all(repo, "line 1.3")
    git(repo, "switch", "build")
    return repo


def read_default(repo: Path) -> str:
    data = json.loads((repo / "config/policies.json").read_text(encoding="utf-8"))
    return data["unofficial_source_policy"]["default_line"]


def test_selection_is_dry_run_by_default_and_idempotent() -> None:
    repo = make_repo()
    try:
        require(not select_line(repo, "1.3", write=False), "dry run reported a write")
        require(read_default(repo) == "1.2", "dry run changed default line")
        require(select_line(repo, "1.3", write=True), "write did not change default line")
        require(read_default(repo) == "1.3", "write selected the wrong default line")
        require(not select_line(repo, "1.3", write=True), "matching selection was not idempotent")
    finally:
        shutil.rmtree(repo)


def test_selection_rejects_unknown_or_misnamed_lines() -> None:
    repo = make_repo()
    try:
        try:
            select_line(repo, "1.4", write=True)
        except ReconstructionError as error:
            require("available lines: 1.2, 1.3" in str(error), "unknown-line error lacks choices")
        else:
            raise AssertionError("unknown line was accepted")

        path = repo / "config" / "policies.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["unofficial_source_policy"]["lines"]["1.3"]["current_ref"] = "source/unofficial/current"
        path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        try:
            select_line(repo, "1.3", write=True)
        except ReconstructionError as error:
            require("expected source/unofficial/1.3/current" in str(error), "misnamed-ref error is unclear")
        else:
            raise AssertionError("misnamed line ref was accepted")
    finally:
        shutil.rmtree(repo)


def test_selected_current_ref_has_no_ambiguous_legacy_fallback() -> None:
    repo = make_repo()
    try:
        path = repo / "config" / "policies.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["unofficial_source_policy"] = {
            "default_line": "",
            "lines": {},
            "prefer_versioned_default_alias": True,
        }
        path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        try:
            selected_unofficial_current_ref(repo)
        except ReconstructionError as error:
            require(
                "default_line" in str(error),
                "missing-line error does not identify the missing policy selection",
            )
        else:
            raise AssertionError("missing default line fell back to source/unofficial/current")
    finally:
        shutil.rmtree(repo)


def load_tests(_loader, _tests, _pattern):
    return load_function_tests(globals())


def main() -> None:
    test_selection_is_dry_run_by_default_and_idempotent()
    test_selection_rejects_unknown_or_misnamed_lines()
    test_selected_current_ref_has_no_ambiguous_legacy_fallback()
    print("unofficial default-line selection tests passed")


if __name__ == "__main__":
    main()
