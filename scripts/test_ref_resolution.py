#!/usr/bin/env python3
"""Regression tests for cached source-ref resolution."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from reconstruction_common import resolve_ref
from test_support import commit_all, git, load_function_tests, write_file


def test_unofficial_release_tag_short_name_resolves() -> None:
    with tempfile.TemporaryDirectory(prefix="edk2-cix-ref-resolution-test.") as raw:
        repo = Path(raw)
        git(repo, "init", "-q")
        git(repo, "config", "user.name", "Test User")
        git(repo, "config", "user.email", "ref-resolution-test")
        write_file(repo, "fixture", "release\n")
        commit_all(repo, "fixture")
        tag = "source/unofficial/edk2/stable-202608"
        git(repo, "tag", tag)

        assert resolve_ref(repo, tag) == f"refs/tags/{tag}"


def test_source_branch_resolution_does_not_fall_back_to_arbitrary_tag() -> None:
    with tempfile.TemporaryDirectory(prefix="edk2-cix-ref-resolution-test.") as raw:
        repo = Path(raw)
        git(repo, "init", "-q")
        git(repo, "config", "user.name", "Test User")
        git(repo, "config", "user.email", "ref-resolution-test")
        write_file(repo, "fixture", "release\n")
        commit_all(repo, "fixture")
        tag = "source/base/edk2/edk2-stable202608"
        git(repo, "tag", tag)

        assert resolve_ref(repo, tag, check=False) is None


def main() -> None:
    test_unofficial_release_tag_short_name_resolves()
    test_source_branch_resolution_does_not_fall_back_to_arbitrary_tag()


def load_tests(_loader: unittest.TestLoader, _tests: unittest.TestSuite, _pattern: str | None) -> unittest.TestSuite:
    return load_function_tests(globals())


if __name__ == "__main__":
    main()
