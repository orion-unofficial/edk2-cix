#!/usr/bin/env python3
"""Compile the real DebugLib headers to verify logging-only RELEASE semantics."""

import os
from pathlib import Path
import platform
import posixpath
import re
import shutil
import subprocess
import tempfile
import unittest

from reconstruction_common import for_each_ref, show_file
from prepare_release_logging import prepare

ROOT = Path(__file__).resolve().parents[1]
WRAPPER = "custom/release-logging/Library/DebugLib.h"
PROBE = r'''
#include <assert.h>
#include <Base.h>
#include <Library/DebugLib.h>
#pragma GCC visibility push(default)
extern int puts (const char *);
#pragma GCC visibility pop

static unsigned Prints, Effects;
static BOOLEAN Enabled = TRUE;
static UINTN Mask = DEBUG_ERROR;
BOOLEAN EFIAPI DebugPrintEnabled (VOID) { return Enabled; }
BOOLEAN EFIAPI DebugPrintLevelEnabled (UINTN Level) { return (Level & Mask) != 0; }
VOID EFIAPI DebugPrint (UINTN Level, CONST CHAR8 *Format, ...) {
  (void)Level;
  ++Prints;
  puts(Format);
}

// These are deliberately undefined: a logging-only build must not reference them,
// even at -O0 with no LTO and regardless of the library's run-time property mask.
extern int AssertOnly (void);
extern void CodeOnly (void);

int main (void) {
  DEBUG ((DEBUG_ERROR, "logging survives", ++Effects));
  DEBUG ((DEBUG_INFO, "masked print", ++Effects));
  Mask = DEBUG_BM;
  DEBUG ((DEBUG_INFO | DEBUG_BM, "boot progress via BM", ++Effects));
  Mask = DEBUG_INFO;
  DEBUG ((DEBUG_INFO | DEBUG_BM, "boot progress via INFO", ++Effects));
  Mask = DEBUG_ERROR;
  Enabled = FALSE;
  DEBUG ((DEBUG_ERROR, "disabled print", ++Effects));
  ASSERT (AssertOnly ());
  ASSERT_EFI_ERROR (AssertOnly ());
  ASSERT_RETURN_ERROR (AssertOnly ());
  ASSERT_PROTOCOL_ALREADY_INSTALLED (0, 0);
  assert (AssertOnly ());
  DEBUG_CODE (CodeOnly (););
  DEBUG_CODE_BEGIN ();
    CodeOnly ();
  DEBUG_CODE_END ();
  DEBUG_CLEAR_MEMORY ((void *)(UINTN)AssertOnly (), AssertOnly ());
#ifndef MDEPKG_NDEBUG
  CodeOnly ();
#endif
#ifndef NDEBUG
  CodeOnly ();
#endif
  return Prints != 3 || Effects != 3;
}
'''


class ReleaseLoggingTests(unittest.TestCase):
    def test_real_make_workspace_switches_select_only_custom_release_logging(self):
        ref = os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current")
        makefile = show_file(ROOT, ref, "src/Makefile").decode()
        start = re.search(r"\.SILENT: Build/%/\$\(UEFI_TARGET\)_GCC5?/FV/SKY1_BL33_UEFI.fd", makefile).start()
        recipe = makefile[start:]
        recipe = recipe[recipe.index('\tif [[ "$(ARTEFACT_MODE)" == "custom"'):
                        recipe.index('\tif [[ "$(CCACHE_ENABLED_EFFECTIVE)"')]
        with tempfile.TemporaryDirectory(prefix="release-logging-make-") as temp:
            root = Path(temp)
            imported = root / "imported/MdePkg"
            header = imported / "Include/Library/DebugLib.h"
            header.parent.mkdir(parents=True)
            original = show_file(ROOT, ref, "src/edk2/MdePkg/Include/Library/DebugLib.h")
            header.write_bytes(original)
            (imported / "MdePkg.dec").write_text("package descriptor\n")
            policy = root / "policy/Library/DebugLib.h"
            policy.parent.mkdir(parents=True)
            policy.write_bytes(show_file(ROOT, ref, WRAPPER))
            (root / "scripts").mkdir()
            (root / "scripts/prepare_release_logging.py").write_bytes(
                show_file(ROOT, ref, "src/scripts/prepare_release_logging.py"))
            # Reuse one workspace, including true -> false -> true transitions.
            cases = (("custom", "RELEASE", "TRUE"), ("custom", "RELEASE", "FALSE"),
                     ("custom", "DEBUG", "TRUE"), ("upstream", "RELEASE", "TRUE"),
                     ("custom", "RELEASE", "TRUE"))
            for mode, target, verbose in cases:
                values = {
                    "ARTEFACT_MODE": mode, "UEFI_TARGET": target, "DEBUG_VERBOSE_NORMALIZED": verbose,
                    "CUSTOM_EDK2_WORKSPACE": str(root / "workspace"),
                    "EDK2_PACKAGES_PATH": str(root / "imported"),
                    "IMPORTED_EDK2_PACKAGES_PATH": str(root / "imported"),
                    "abspath edk2/MdePkg": str(imported),
                    "CUSTOM_RELEASE_LOGGING_INCLUDE": str(root / "policy"),
                    "shell pwd": str(root),
                }
                command = re.sub(r"\$\(([^()]+)\)", lambda match: values.get(match[1], "FALSE"), recipe)
                command = command.replace("$$", "$").replace("$*", "O6")
                command += '\nprintf "%s\\n" "$PACKAGES_PATH"\n'
                result = subprocess.run(["bash", "-ec", command], cwd=root, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                selected = "logging-overlay" in result.stdout
                self.assertEqual(selected, mode == "custom" and target == "RELEASE" and verbose == "TRUE")
                self.assertEqual(header.read_bytes(), original)

    def test_logging_only_compiles_against_every_retained_header_version(self):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "a C compiler is required for RELEASE logging qualification")
        selected = os.environ.get("SOURCE_TEST_REF")
        arch = "AArch64" if platform.machine().lower() in ("arm64", "aarch64") else "X64"
        seen = set()
        for ref in for_each_ref(ROOT, "source/unofficial/"):
            wrapper = show_file(ROOT, selected or ref, WRAPPER)
            headers = {name: show_file(ROOT, ref, "src/edk2/MdePkg/Include/" + name)
                       for name in ("Base.h", f"{arch}/ProcessorBind.h", "Library/DebugLib.h")}
            fingerprint = tuple(headers.values()) + (wrapper,)
            if fingerprint in seen:
                continue
            seen.add(fingerprint)
            with self.subTest(ref=ref, arch=arch), tempfile.TemporaryDirectory(prefix="release-logging-") as temp:
                root = Path(temp)
                for name, data in headers.items():
                    path = root / "imported/Include" / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
                path = root / "policy.h"
                path.write_bytes(wrapper)
                (root / "imported/MdePkg.dec").write_text("package descriptor\n")
                prepare(root / "imported", path, root / "logging")
                # Regeneration must replace an older policy rather than retaining it.
                generated = root / "logging/MdePkg/Include/Library/DebugLib.h"
                generated.write_bytes(b"stale header")
                prepare(root / "imported", path, root / "logging")
                self.assertEqual(generated.read_bytes(), headers["Library/DebugLib.h"] + b"\n" + wrapper)
                for name, data in headers.items():
                    self.assertEqual((root / "imported/Include" / name).read_bytes(), data)
                self.assertEqual((root / "logging/MdePkg/MdePkg.dec").read_text(), "package descriptor\n")
                source = root / "probe.c"
                source.write_text(PROBE)
                for optimization in ("-O0", "-Os"):
                    result = subprocess.run([
                        compiler, optimization, "-Wall", "-Wextra", "-Werror", "-fshort-wchar",
                        "-DMDEPKG_NDEBUG", "-DNDEBUG", "-I" + str(root / "logging/MdePkg/Include"),
                        "-I" + str(root / "logging/MdePkg/Include" / arch),
                        str(source), "-o", str(root / "probe"),
                    ], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run([str(root / "probe")], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(result.stdout, "logging survives\nboot progress via BM\nboot progress via INFO\n")

    def test_unknown_header_shape_cannot_silently_disable_logging(self):
        with tempfile.TemporaryDirectory(prefix="release-logging-unknown-") as temp:
            root = Path(temp)
            header = root / "MdePkg/Include/Library/DebugLib.h"
            header.parent.mkdir(parents=True)
            header.write_text("unsupported header\n")
            policy = root / "policy.h"
            policy.write_text("policy\n")
            with self.assertRaisesRegex(ValueError, "unsupported upstream"):
                prepare(root / "MdePkg", policy, root / "output")
            self.assertFalse((root / "output").exists())

    def test_public_source_paths_keep_release_definitions(self):
        ref = os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current")
        for board in ("O6", "O6N"):
            path = f"custom/overlay/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc"
            text = show_file(ROOT, ref, path).decode()
            if text.startswith("../"):
                path = posixpath.normpath(posixpath.join(posixpath.dirname(path), text.strip()))
                text = show_file(ROOT, ref, path).decode()
            self.assertNotIn("-UMDEPKG_NDEBUG", text)
            self.assertIn("-DMDEPKG_NDEBUG -DNDEBUG", text)
        for overlay in ("overlay", "overlay-experimental-uefi-settings"):
            common = show_file(ROOT, ref, f"custom/{overlay}/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc").decode()
            self.assertNotIn("-UMDEPKG_NDEBUG", common)
            self.assertIn("gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x02", common)
        makefile = show_file(ROOT, ref, "src/Makefile").decode()
        self.assertIn("$(if $(filter custom,$(ARTEFACT_MODE)),$(CUSTOM_RELEASE_LOGGING_SOURCES) $(CUSTOM_BL33_SOURCES))", makefile)
        self.assertIn('"$(UEFI_TARGET)" == "RELEASE" && "$(DEBUG_VERBOSE_NORMALIZED)" == "TRUE"', makefile)
        self.assertIn('export PACKAGES_PATH="$$WORKSPACE/logging-overlay:$$PACKAGES_PATH"', makefile)


if __name__ == "__main__":
    unittest.main()
