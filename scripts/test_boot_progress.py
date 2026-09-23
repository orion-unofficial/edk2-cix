#!/usr/bin/env python3
"""Keep boot-progress retagging narrow and upstream-derived overlays complete."""

import os
from pathlib import Path
import posixpath
import re
import unittest

from reconstruction_common import for_each_ref, show_file

ROOT = Path(__file__).resolve().parents[1]


class BootProgressTests(unittest.TestCase):
    def test_retained_sources_preserve_original_levels_and_module_inputs(self):
        selected = os.environ.get("SOURCE_TEST_REF")
        refs = [selected] if selected else for_each_ref(ROOT, "source/unofficial/")
        for ref in refs:
            with self.subTest(ref=ref):
                for module, filename, count in (
                    ("Universal/BdsDxe", "BdsEntry.c", 5),
                    ("Library/UefiBootManagerLib", "BmBoot.c", 4),
                ):
                    base = "edk2/MdeModulePkg/" + module
                    original = show_file(ROOT, ref, "src/" + base + "/" + filename)
                    changed = show_file(ROOT, ref, "custom/overlay/" + base + "/" + filename)
                    self.assertEqual(changed.count(b" | DEBUG_BM"), count)
                    # Exact bytes, including CRLF and every existing INFO/LOAD/error level.
                    self.assertEqual(changed.replace(b" | DEBUG_BM", b""), original)
                    inf = module.rsplit("/", 1)[1] + ".inf"
                    descriptor = "custom/overlay/" + base + "/" + inf
                    link = show_file(ROOT, ref, descriptor).decode()
                    self.assertEqual(posixpath.normpath(posixpath.join(posixpath.dirname(descriptor), link)),
                                     "src/" + base + "/" + inf)
                    text = show_file(ROOT, ref, "src/" + base + "/" + inf).decode()
                    sources = text.split("[Sources]", 1)[1].split("[", 1)[0]
                    for filename in sources.splitlines():
                        filename = filename.strip()
                        if not filename or filename.startswith("#"):
                            continue
                        path = "custom/overlay/" + base + "/" + filename
                        self.assertTrue(show_file(ROOT, ref, path), path)
                platform = show_file(ROOT, ref, "custom/overlay/edk2-platforms/Platform/CIX/Sky1/"
                                     "Library/PlatformBootManagerLib/PlatformBm.c").decode()
                self.assertEqual(platform.count("DEBUG_INFO | DEBUG_BM"), 3)
                self.assertGreaterEqual(platform.count('DebugPrint (DEBUG_INIT | DEBUG_BM, "[BDS]'), 10)
                self.assertFalse(re.search(r"DEBUG_ERROR\s*\|\s*DEBUG_BM", platform))


if __name__ == "__main__":
    unittest.main()
