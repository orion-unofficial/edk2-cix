"""Historical recipe insertions reject ambiguous anchors without changing outputs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from reconstruction_common import ReconstructionError
from render_release_branch import add_dependency_package, add_vendor_asl_compat


class HistoricalInsertionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='render-compat-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)

    def test_dependency_insertion_is_exact_and_idempotent(self):
        path = self.root / 'dependencies.sh'
        path.write_text('packages=(\n    anchor-package\n)\n')
        add_dependency_package(self.root, path.name, 'anchor-package', 'compat-package', False)
        expected = b'packages=(\n    anchor-package\n    compat-package\n)\n'
        self.assertEqual(path.read_bytes(), expected)
        add_dependency_package(self.root, path.name, 'anchor-package', 'compat-package', False)
        self.assertEqual(path.read_bytes(), expected)

    def test_missing_or_duplicate_dependency_anchors_are_rejected(self):
        for text in ('no anchor\n', '    anchor-package\n    anchor-package\n'):
            path = self.root / 'dependencies.sh'
            path.write_text(text)
            with self.assertRaisesRegex(ReconstructionError, 'missing or ambiguous'):
                add_dependency_package(self.root, path.name, 'anchor-package', 'compat-package', False)
            self.assertEqual(path.read_text(), text)

    def test_asl_flags_are_inserted_in_the_active_block(self):
        path = self.root / 'Makefile'
        old = '\t\t\tupstream) \\\n\t\t\t\t;; \\\n'
        # An unrelated comment cannot stand in for active compiler flags.
        path.write_text('# elsewhere: -vw 3073\n' + old)
        add_vendor_asl_compat(self.root, path.name, ['3073'], False)
        result = path.read_bytes()
        self.assertIn(b'DEBUG_GCC5_AARCH64_ASL_FLAGS   = -vw 3073', result)
        self.assertIn(b'RELEASE_GCC5_AARCH64_ASL_FLAGS   = -vw 3073', result)
        add_vendor_asl_compat(self.root, path.name, ['3073'], False)
        self.assertEqual(path.read_bytes(), result)

    def test_missing_or_duplicate_asl_blocks_are_rejected(self):
        old = '\t\t\tupstream) \\\n\t\t\t\t;; \\\n'
        for text in ('no active block\n', old + old):
            path = self.root / 'Makefile'
            path.write_text(text)
            with self.assertRaisesRegex(ReconstructionError, 'missing or ambiguous'):
                add_vendor_asl_compat(self.root, path.name, ['3073'], False)
            self.assertEqual(path.read_text(), text)


if __name__ == '__main__':
    unittest.main()
