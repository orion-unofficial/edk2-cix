#!/usr/bin/env python3
"""Exercise real recursive Make argument forwarding for batch compiler jobs."""

from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile
import unittest

from parallel_batch_make import profile


class ParallelMakeTests(unittest.TestCase):
    def test_profile_rejects_invalid_job_counts(self):
        for jobs in (0, -1, 4097, True, "4"):
            with self.subTest(jobs=jobs), self.assertRaises(ValueError):
                profile(jobs)

    def test_recursive_make_preserves_flags_and_packaging(self):
        with tempfile.TemporaryDirectory(prefix="parallel-batch-make-") as tmp:
            root = Path(tmp).resolve()
            options = root / "compiler options.mk"
            options.write_text(profile(4))
            (root / "Makefile").write_text(
                "SRC_COMMON_ARGS = BOARD=O6 FIXES=TRUE MASK=0x80000001\n"
                "PACKAGING = --no-genfds-multi-thread\n"
                "all:\n\t@$(MAKE) --no-print-directory child\n"
                "child:\n\t@printf '%s\\n' '$(SRC_COMMON_ARGS)' '$(PACKAGING)'\n")
            helper = Path(__file__).with_name("parallel_batch_make.py").resolve()
            wrapper = shlex.join([sys.executable, str(helper), "--jobs", "4", "--profile", str(options), "--"])
            for mode in ("custom", "upstream"):
                result = subprocess.run(["/usr/bin/make", "-s", "all", "MAKE=" + wrapper,
                                         "ARTEFACT_MODE=" + mode], cwd=root, capture_output=True,
                                        text=True, check=True, env=dict(os.environ, MAKEFLAGS=""))
                lines = result.stdout.splitlines()
                expected = "BOARD=O6 FIXES=TRUE MASK=0x80000001"
                if mode == "custom":
                    expected += " EDK2_BUILD_JOBS=4 BASETOOLS_BUILD_JOBS=4 HELPER_BUILD_JOBS=4"
                self.assertEqual(lines, [expected, "--no-genfds-multi-thread"])
            options.write_text(profile(3))
            result = subprocess.run([sys.executable, str(helper), "--jobs", "4", "--profile", str(options),
                                     "--", "all"], cwd=root, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("differs from recorded", result.stderr)


if __name__ == "__main__":
    unittest.main()
