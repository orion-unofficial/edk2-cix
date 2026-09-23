#!/usr/bin/env python3
"""Keep the delegated ACPI/FV audit diagnostics under the build quality gate."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from test_firmware_update_safety import source


class SourceAuditSummaryTests(unittest.TestCase):
    def test_selected_source_audit_regressions(self):
        names = ('audit_summary.py', 'audit_acpi_regression.py', 'audit_final_image_manifest.py',
                 'test_audit_summary.py', 'test_audit_acpi_regression.py', 'test_audit_final_image_manifest.py', 'check_custom_overlay_inf_sync.py',
                 'test_check_custom_overlay_inf_sync.py')
        with tempfile.TemporaryDirectory(prefix='source-audit-summary-') as directory:
            root = Path(directory)
            for name in names:
                (root / name).write_text(source('scripts/' + name))
            result = subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(root), '-p', 'test_*.py'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
