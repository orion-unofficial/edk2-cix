#!/usr/bin/env python3
"""Unit checks for version-pinned GitHub release resolution."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from resolve_release_asset import resolve_release_asset


class ResolveReleaseAssetTests(unittest.TestCase):
    @patch("resolve_release_asset.github_request")
    def test_exact_versioned_package_is_preferred(self, request) -> None:
        request.return_value = {
            "assets": [
                {"name": "unrelated.deb"},
                {"name": "edk2-cix_1.2.1+test_all.deb", "id": 42},
            ]
        }

        asset = resolve_release_asset("radxa-pkg/edk2-cix", "1.2.1+test", None)

        self.assertEqual(asset["id"], 42)
        self.assertIn("/releases/tags/1.2.1%2Btest", request.call_args.args[0])

    @patch("resolve_release_asset.github_request")
    def test_single_debian_package_is_an_unambiguous_fallback(self, request) -> None:
        request.return_value = {
            "assets": [
                {"name": "README.txt"},
                {"name": "vendor-package.deb", "id": 7},
            ]
        }

        asset = resolve_release_asset("radxa-pkg/edk2-cix", "1.2.1", "token")

        self.assertEqual(asset["id"], 7)
        self.assertEqual(request.call_args.args[1], "token")

    @patch("resolve_release_asset.github_request")
    def test_ambiguous_debian_packages_are_rejected(self, request) -> None:
        request.return_value = {
            "assets": [{"name": "one.deb"}, {"name": "two.deb"}]
        }

        with self.assertRaisesRegex(RuntimeError, "multiple .deb assets"):
            resolve_release_asset("radxa-pkg/edk2-cix", "1.2.1", None)

    def test_invalid_repository_or_tag_is_rejected_before_network_access(self) -> None:
        with self.assertRaisesRegex(ValueError, "owner/name"):
            resolve_release_asset("https://example.invalid/repo", "1.2.1", None)
        with self.assertRaisesRegex(ValueError, "Invalid release tag"):
            resolve_release_asset("radxa-pkg/edk2-cix", "latest", None)


if __name__ == "__main__":
    unittest.main()
