#!/usr/bin/env python3
"""Keep reserved fixture identities distinct from private metadata leaks."""
from pathlib import Path
from types import SimpleNamespace
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

    def test_assistant_name_is_allowed_only_in_root_agent_instructions(self):
        name = "Co" + "dex"
        self.assertEqual(self.scan("AGENTS.md", name), [])
        for path in ("src/example.c", "docs/guide.md", "config/example.json",
                     "docs/AGENTS.md", "scripts/AGENTS.md", "agents.md"):
            with self.subTest(path=path):
                problems = self.scan(path, name)
                self.assertEqual(len(problems), 1)
                self.assertIn("generated assistant identity", problems[0])

    def test_agent_instructions_still_reject_other_identity_patterns(self):
        for value, label in (
            ("/" + "Users/fixture/project", "host path"),
            ("/" + "private/" + "tmp/fixture", "host path"),
            ("/" + "tmp/fixture", "host path"),
            ("fixture" + "@" + "example.org", "embedded email"),
            ("fixture" + "@" + "example.invalid", "embedded email"),
            ("main-" + "monorepo", "old branch name"),
        ):
            with self.subTest(label=label, value=value):
                problems = self.scan("AGENTS.md", "Co" + "dex\n" + value)
                self.assertEqual(len(problems), 1)
                self.assertIn(label, problems[0])

    def test_assistant_name_is_rejected_in_each_commit_metadata_field(self):
        clean = ["a" * 40, "Fixture Author", "author@example.invalid",
                 "Fixture Committer", "committer@example.invalid", "Fixture subject"]
        for field in range(1, 6):
            with self.subTest(field=field):
                fields = clean.copy()
                fields[field] = "Co" + "dex"
                result = SimpleNamespace(stdout="\0".join(fields) + "\n")
                with patch.object(identity, "git", return_value=result):
                    problems = identity.scan_commits(Path("."))
                self.assertEqual(len(problems), 1)
                self.assertIn("generated identity string in commit metadata", problems[0])
        with patch.object(identity, "git", return_value=SimpleNamespace(stdout="\0".join(clean) + "\n")):
            self.assertEqual(identity.scan_commits(Path(".")), [])

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
