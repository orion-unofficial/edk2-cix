#!/usr/bin/env python3
"""Verify hybrid build cache invalidation and retained-executable checks."""
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import build_hybrid_stmm as hybrid
from stage_vendor_stmm import StmmError, sha256


class HybridTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='hybrid-stmm-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / 'component'
        self.vendor = self.source / 'CixStandaloneMmPkg/Vendor'
        self.vendor.mkdir(parents=True)
        self.binary = b'vendor-executable'
        (self.vendor / 'driver.efi').write_bytes(self.binary)
        (self.vendor / 'manifest.json').write_text(json.dumps({
            'schema_version': 1, 'files': {'driver.efi': sha256(self.binary)}}))
        self.payload = self.binary + bytes(2 * 1024 * 1024 - len(self.binary))
        self.builds = 0

    def compile(self, command, **kwargs):
        self.builds += 1
        output = Path(kwargs['cwd']) / 'Build/StandaloneMm/RELEASE_GCC/FV/BL32_AP_MM.fd'
        output.parent.mkdir(parents=True)
        output.write_bytes(self.payload)

    def build(self):
        with patch.object(hybrid, 'compiler_fingerprint', return_value='compiler'), \
                patch.object(hybrid.subprocess, 'run', side_effect=self.compile):
            return hybrid.build(self.source, self.root / 'cache', 'cross-', 2)

    def test_cache_reuse_source_change_and_corruption(self):
        first, payload = self.build()
        self.assertEqual(payload, self.payload)
        self.assertEqual(self.build()[0], first)
        self.assertEqual(self.builds, 1)
        cached = next((self.root / 'cache').glob('*/BL32_AP_MM.fd'))
        cached.write_bytes(b'corrupt')
        self.assertEqual(self.build()[1], self.payload)
        self.assertEqual(self.builds, 2)
        (self.source / 'changed.c').write_text('source change\n')
        self.assertNotEqual(self.build()[0]['source'], first['source'])
        self.assertEqual(self.builds, 3)

    def test_outer_make_overrides_are_not_inherited(self):
        def compile_isolated(command, **kwargs):
            environment = kwargs['env']
            for name in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES', 'PACKAGES_PATH', 'CONF_PATH'):
                self.assertNotIn(name, environment)
            self.assertEqual(environment['SOURCE_DATE_EPOCH'], '0')
            self.compile(command, **kwargs)
        with patch.dict(os.environ, MAKEFLAGS='SOURCE_DATE_EPOCH=123', MFLAGS='-e',
                        MAKEOVERRIDES='SOURCE_DATE_EPOCH=123', PACKAGES_PATH='/other', CONF_PATH='/other'), \
                patch.object(hybrid, 'compiler_fingerprint', return_value='compiler'), \
                patch.object(hybrid.subprocess, 'run', side_effect=compile_isolated):
            hybrid.build(self.source, self.root / 'cache', 'cross-', 2)

    def test_modified_vendor_executable_rejected_before_build(self):
        (self.vendor / 'driver.efi').write_bytes(b'modified')
        with self.assertRaisesRegex(StmmError, 'hash mismatch'):
            self.build()
        self.assertEqual(self.builds, 0)

    def test_built_image_must_retain_vendor_executable_once(self):
        self.payload = bytes(2 * 1024 * 1024)
        with self.assertRaisesRegex(StmmError, 'exactly one unchanged'):
            self.build()


if __name__ == '__main__':
    unittest.main()
