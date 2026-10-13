#!/usr/bin/env python3
"""Keep reserved fixture identities distinct from private metadata leaks."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import check_identity_integrity as identity
from publish_source_update import ORION_EMAILS, ORION_PUSH_URLS


class FixtureEmailTests(unittest.TestCase):
    def scan_bytes(self, path, data):
        with tempfile.TemporaryDirectory(prefix="identity-fixture-") as directory:
            root = Path(directory)
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            with patch.object(identity, "tracked_files", return_value=[path]):
                return identity.scan_files(root, False)

    def scan(self, path, address):
        return self.scan_bytes(path, (address + "\n").encode())

    def test_only_exact_imported_attribution_is_exempt(self):
        path = "scripts/tests/fixtures/radxa13-source-conflicts/Sky1Common.dsc.inc"
        data = (Path(__file__).resolve().parents[1] / path).read_bytes()
        self.assertEqual(self.scan_bytes(path, data), [])
        for name, changed in (
            (path, data + b"\n# new author <fixture" + b"@" + b"example.org>\n"),
            (path, data.replace(b"Copyright 2024", b"Copyright 2025")),
            (path, data.replace(b"@gmail.com", b"@example.org")),
            (path + ".copy", data),
        ):
            with self.subTest(path=name, changed=changed != data):
                self.assertTrue(any("embedded email" in item for item in self.scan_bytes(name, changed)))

    def test_imported_attribution_never_exempts_other_patterns(self):
        path = next(iter(identity.IMPORTED_ATTRIBUTIONS))
        data = (Path(__file__).resolve().parents[1] / path).read_bytes()
        with patch.object(identity, "suspicious_patterns", return_value=[
                ("host path", identity.re.compile("Copyright"))]):
            self.assertTrue(any("host path" in item for item in self.scan_bytes(path, data)))

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

    def test_publication_identities_are_limited_to_guard_test_and_guide(self):
        self.assertEqual(identity.PUBLICATION_IDENTITIES, ORION_EMAILS | {
            url.removeprefix("https://").split("/")[0] for url in ORION_PUSH_URLS if "@" in url
        })
        for path in identity.PUBLICATION_IDENTITY_FILES:
            for email in identity.PUBLICATION_IDENTITIES:
                with self.subTest(path=path, email=email):
                    self.assertEqual(self.scan(path, email), [])
            self.assertTrue(self.scan(path, "other" + "@" + "example.org"))
        for path in ("scripts/other.py", "docs/other.md", "config/other.json"):
            for email in identity.PUBLICATION_IDENTITIES:
                with self.subTest(path=path, email=email):
                    self.assertTrue(self.scan(path, email))

    def test_publication_exception_does_not_exempt_other_patterns(self):
        email = next(iter(identity.PUBLICATION_IDENTITIES))
        with patch.object(identity, "suspicious_patterns", return_value=[
                ("host path", identity.re.compile(identity.re.escape(email)))]):
            self.assertTrue(self.scan("scripts/publish_source_update.py", email))


if __name__ == "__main__":
    unittest.main()
