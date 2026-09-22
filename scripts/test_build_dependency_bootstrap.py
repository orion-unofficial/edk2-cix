#!/usr/bin/env python3

import pathlib
import re
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


def read_repo_text(relative_path: str) -> str:
    return (REPO_ROOT / relative_path).read_text(encoding="utf-8")


class BuildDependencyBootstrapTest(unittest.TestCase):
    def test_native_arm64_dependency_bootstrap_does_not_pull_in_qemu_packages(self) -> None:
        content = read_repo_text("scripts/ensure_build_deps.sh")
        common_packages = re.search(r"common_packages=\(\n(?P<body>.*?)\n\)", content, re.S)
        self.assertIsNotNone(common_packages)
        common_body = common_packages.group("body")
        self.assertNotIn("binfmt-support", common_body)
        self.assertNotIn("qemu-user-static", common_body)
        self.assertIn('"${emulation_packages[@]}"', common_body)

        self.assertIn("emulation_packages+=(", content)
        self.assertIn("            binfmt-support", content)
        self.assertIn("            qemu-user-static", content)

    def test_distclean_also_removes_the_managed_ccache_directory(self) -> None:
        content = read_repo_text("src/Makefile")
        self.assertIn("__distclean: __clean __clean-ccache", content)

    def test_ccache_support_is_not_limited_to_custom_artefacts(self) -> None:
        content = read_repo_text("src/Makefile")
        self.assertIn('CCACHE_BIN ?= ccache', content)
        self.assertIn('if [[ "$(CCACHE_ENABLED_EFFECTIVE)" == "TRUE" ]]; then', content)
        self.assertNotIn('ARTEFACT_MODE)" == "custom" && -n "$(strip $(CCACHE_BIN))"', content)


if __name__ == "__main__":
    unittest.main()
