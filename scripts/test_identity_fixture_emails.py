#!/usr/bin/env python3
"""Keep reserved fixture identities distinct from private metadata leaks."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import check_identity_integrity as identity


class FixtureEmailTests(unittest.TestCase):
    def scan(self, path, address):
        with tempfile.TemporaryDirectory(prefix="identity-fixture-") as directory:
            root = Path(directory)
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(address + "\n")
            with patch.object(identity, "tracked_files", return_value=[path]):
                return identity.scan_files(root, False)

    def test_only_reserved_test_fixture_domain_is_exempt(self):
        reserved = "fixture" + "@" + "example.invalid"
        self.assertEqual(self.scan("scripts/test_fixture.py", reserved), [])
        for path, address in (
            ("scripts/test_fixture.py", "fixture" + "@" + "example.org"),
            ("scripts/test_fixture.py", reserved + ".example.org"),
            ("scripts/fixture.py", reserved),
            ("docs/test_fixture.py", reserved),
            ("scripts/test_fixture.md", reserved),
        ):
            with self.subTest(path=path, address=address):
                problems = self.scan(path, address)
                self.assertEqual(len(problems), 1)
                self.assertIn("embedded email", problems[0])

    def test_other_identity_checks_still_apply_to_reserved_fixture(self):
        address = "co" + "dex" + "@" + "example.invalid"
        problems = self.scan("scripts/test_fixture.py", address)
        self.assertEqual(len(problems), 1)
        self.assertIn("generated assistant identity", problems[0])


if __name__ == "__main__":
    unittest.main()
