#!/usr/bin/env python3
"""Exercise the rendered Make rules with fixture sources and a BL2 builder."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from reconstruction_common import for_each_ref, show_file
from test_firmware_update_safety import source


class CixReleaseDependencyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory(prefix="cix-release-dependencies-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.src = self.root / "src"
        self.src.mkdir()
        makefile = source("src/Makefile")
        self.toolchain = "GCC5" if "BUILD_OUTPUT_DIR = Build/$1/$(UEFI_TARGET)_GCC5" in makefile else "GCC"
        self.has_tf_a_fixes = '"--enable-tf-a-fixes"' in source("scripts/cix_release_cache.py")
        start = makefile.index("# Hash the selected sources on every build")
        end = makefile.index(".SILENT: $(call BUILD_STAGE_DIR,%)/Firmwares/bootloader3.img", start)
        names = (
            "BUILD_OUTPUT_DIR", "BUILD_STAGE_DIR", "BUILD_CONFIG_STAMP",
            "CIX_RELEASE_BOOTLOADER2_STAMP", "ACTIVE_CIX_RELEASE_BOOTLOADER2_STAMP",
        )
        definitions = "\n".join(line for line in makefile.splitlines() if line.split(" = ")[0] in names)
        self.write("scripts/cix_release_cache.py", source("scripts/cix_release_cache.py"))
        self.write("src/tfa/Makefile", "# TF-A\n")
        self.write("src/tfa/tools/cert_create/Makefile", "# cert_create\n")
        self.write("src/tfa/plat/main.c", "int value = 1;\n")
        self.write("src/tee/Makefile", "# OP-TEE\n")
        self.write("src/tee/core/main.c", "int value = 2;\n")
        self.write("src/compiler", "#!/bin/sh\necho fixture-compiler-v1\n", executable=True)
        self.write("src/fiptool", "fixture\n")
        self.write("src/builder.sh", '''#!/bin/bash
set -eu
output=
while [ "$#" -gt 0 ]; do
    if [ "$1" = --output ]; then output="$2"; fi
    shift
done
if [ "${FAIL_BUILDER:-0}" = 1 ]; then exit 1; fi
printf 'build\\n' >> build.log
cp "$(dirname "$(dirname "$output")")/.cix-release-bootloader2-inputs" "$output"
''')
        self.write("src/Makefile", '''SHELL := /bin/bash
.SHELLFLAGS := -eo pipefail -c
.ONESHELL:
.DELETE_ON_ERROR:
FIRMWARE_TARGET ?= RELEASE
UEFI_TARGET = $(FIRMWARE_TARGET)
ARTEFACT_MODE ?= custom
CIX_RELEASE_NORMALIZED ?= 1.2
FIRMWARE_BOARDS := O6 O6N
CIX_RELEASE_CACHE_HELPER := $(abspath ../scripts/cix_release_cache.py)
CIX_RELEASE_BOOTLOADER2_HELPER := builder.sh
CIX_RELEASE_V12_TFA_SOURCE_DIR := tfa
CIX_RELEASE_V12_TEE_SOURCE_DIR := tee
CIX_RELEASE_SIGNING_KEYS_DIR := keys
CIX_RELEASE_CACHE_ROOT := $(abspath ../cache)
GCC_AARCH64_PREFIX := missing-fixture-cross-
FIPTOOL_BIN := fiptool
HELPER_BUILD_JOBS := 1
ENSURE_REPO_LOCK = :
''' + definitions + "\n.PHONY: FORCE\nFORCE:\n" + makefile[start:end] + '''
$(call BUILD_CONFIG_STAMP,%): FORCE
\tmkdir -p "$(@D)"
\ttest -f "$@" || printf 'config\\n' > "$@"
$(call BUILD_STAGE_DIR,%)/Firmwares/dummy.bin: $(call BUILD_CONFIG_STAMP,%)
\tmkdir -p "$(@D)"
\tprintf 'dummy\\n' > "$@"
\tprintf 'vendor BL2\\n' > "$(@D)/bootloader2.img"
\tprintf 'STMM\\n' > "$(@D)/BL32_AP_EFI_STMM.fd"
$(call BUILD_STAGE_DIR,%)/certs/trusted_key_no.crt: $(call BUILD_CONFIG_STAMP,%)
\tmkdir -p "$(@D)"
\tprintf 'cert\\n' > "$@"
.NOTINTERMEDIATE: $(foreach board,$(FIRMWARE_BOARDS),$(call BUILD_CONFIG_STAMP,$(board)) $(call BUILD_STAGE_DIR,$(board))/Firmwares/dummy.bin $(call BUILD_STAGE_DIR,$(board))/certs/trusted_key_no.crt $(call BUILD_STAGE_DIR,$(board))/Firmwares/bootloader2.img)
''')

    def write(self, relative: str, content: str, *, executable: bool = False) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        if executable:
            path.chmod(0o755)
        return path

    def build(self, *variables: str, board: str = "O6", target: str = "RELEASE", success: bool = True):
        result = subprocess.run(
            [shutil.which("gmake") or "make", "--no-print-directory", "-j4",
             f"Build/{board}/{target}_{self.toolchain}/Firmwares/bootloader2.img", f"FIRMWARE_TARGET={target}", *variables],
            cwd=self.src, env={**os.environ, "CC": str(self.src / "compiler")},
            capture_output=True, text=True,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def stamp(self, board: str = "O6", target: str = "RELEASE") -> Path:
        return self.src / f"Build/{board}/{target}_{self.toolchain}.build/.cix-release-bootloader2-inputs"

    def output(self, board: str = "O6", target: str = "RELEASE") -> Path:
        return self.src / f"Build/{board}/{target}_{self.toolchain}/Firmwares/bootloader2.img"

    def count(self) -> int:
        log = self.src / "build.log"
        return len(log.read_text().splitlines()) if log.exists() else 0

    def test_unchanged_and_generated_inputs_do_not_rebuild(self) -> None:
        self.build()
        before = (self.stamp().stat().st_mtime_ns, self.output().stat().st_mtime_ns)
        for path in ("src/tfa/build/sky1/bl31.bin", "src/tfa/plat/main.o",
                     "src/tfa/tools/cert_create/cert_create", "src/tee/out/core/tee-raw.bin",
                     "src/tee/tee.bin", "src/tee/__pycache__/generated.pyc"):
            self.write(path, "generated\n")
        self.build()
        self.assertEqual(self.count(), 1)
        self.assertEqual(before, (self.stamp().stat().st_mtime_ns, self.output().stat().st_mtime_ns))

    def test_tfa_edits_additions_deletions_and_renames_rebuild(self) -> None:
        self.build()
        old = json.loads(self.stamp().read_text())
        path = self.src / "tfa/plat/main.c"
        original_stat = path.stat()
        path.write_text("int value = 3;\n")
        os.utime(path, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        self.build()
        changed = json.loads(self.stamp().read_text())
        self.assertNotEqual(old["bl31_key"], changed["bl31_key"])
        self.assertEqual(old["tee_key"], changed["tee_key"])
        self.assertEqual(old["cert_create_key"], changed["cert_create_key"])
        added = self.write("src/tfa/plat/new.c", "new source\n")
        self.build()
        added.rename(added.with_name("renamed.c"))
        self.build()
        added.with_name("renamed.c").unlink()
        self.build()
        self.assertEqual(self.count(), 5)
        self.assertEqual(self.output().read_bytes(), self.stamp().read_bytes())

    def test_tee_and_stmm_changes_only_invalidate_tee(self) -> None:
        self.build()
        old = json.loads(self.stamp().read_text())
        self.write("src/tee/core/main.c", "int value = 4;\n")
        self.build()
        changed = json.loads(self.stamp().read_text())
        self.assertNotEqual(old["tee_key"], changed["tee_key"])
        self.assertEqual(old["bl31_key"], changed["bl31_key"])
        stmm = self.stamp().parent / "Firmwares/BL32_AP_EFI_STMM.fd"
        stmm.write_bytes(b"new STMM")
        self.build()
        self.assertNotEqual(changed["tee_key"], json.loads(self.stamp().read_text())["tee_key"])
        stmm.unlink()
        self.build()
        self.assertEqual(self.count(), 4)

    def test_options_compiler_and_helper_changes_rebuild(self) -> None:
        self.build()
        if self.has_tf_a_fixes:
            self.build("ENABLE_TF_A_FIXES_NORMALIZED=TRUE")
            self.build("ENABLE_TF_A_FIXES_NORMALIZED=FALSE")
        self.write("src/compiler", "#!/bin/sh\necho fixture-compiler-v2\n", executable=True)
        self.build()
        helper = self.src / "builder.sh"
        helper.write_text(helper.read_text() + "\n# helper change\n")
        self.build()
        expected = 5 if self.has_tf_a_fixes else 3
        self.assertEqual(self.count(), expected)
        self.build(target="DEBUG")
        self.build(board="O6N")
        self.assertEqual(self.count(), expected + 2)
        self.assertNotEqual(json.loads(self.stamp().read_text())["bl31_key"],
                            json.loads(self.stamp(target="DEBUG").read_text())["bl31_key"])
        self.build(target="DEBUG")
        self.build(board="O6N")
        self.assertEqual(self.count(), expected + 2)

    def test_failed_fingerprint_preserves_stamp_and_image(self) -> None:
        self.build()
        stamp = self.stamp().read_bytes()
        image = self.output().read_bytes()
        shutil.rmtree(self.src / "tfa")
        result = self.build(success=False)
        self.assertIn("Missing required source directory", result.stderr)
        self.assertEqual(self.stamp().read_bytes(), stamp)
        self.assertEqual(self.output().read_bytes(), image)

    def test_failed_builder_is_retried(self) -> None:
        self.build()
        before = self.output().read_bytes()
        self.write("src/tfa/plat/main.c", "changed source\n")
        self.build("FAIL_BUILDER=1", success=False)
        self.assertEqual(self.output().read_bytes(), before)
        self.build()
        self.assertEqual(self.count(), 2)
        self.assertEqual(self.output().read_bytes(), self.stamp().read_bytes())

    def test_vendor_path_does_not_fingerprint_sources(self) -> None:
        shutil.rmtree(self.src / "tfa")
        shutil.rmtree(self.src / "tee")
        for variables in (("CIX_RELEASE_NORMALIZED=",), ("CIX_RELEASE_NORMALIZED=", "ARTEFACT_MODE=upstream")):
            self.build(*variables)
            self.assertFalse(self.stamp().exists())
            self.assertEqual(self.count(), 0)
            self.assertEqual(self.output().read_text(), "vendor BL2\n")


class RetainedCixReleaseDependencyTests(unittest.TestCase):
    @unittest.skipIf(os.environ.get("SOURCE_TEST_REF") or os.environ.get("SOURCE_TEST_ROOT"),
                     "The caller selected a source variant")
    def test_distinct_retained_dependency_rules(self) -> None:
        root = Path(__file__).resolve().parents[1]
        variants = {}
        for ref in for_each_ref(root, "source/unofficial/"):
            makefile = show_file(root, ref, "src/Makefile")
            start = makefile.index(b"# Hash the selected sources on every build")
            end = makefile.index(b".SILENT: $(call BUILD_STAGE_DIR,%)/Firmwares/bootloader3.img", start)
            definitions = b"\n".join(line for line in makefile.splitlines() if line.startswith(
                (b"BUILD_OUTPUT_DIR =", b"BUILD_STAGE_DIR =", b"BUILD_CONFIG_STAMP =",
                 b"CIX_RELEASE_BOOTLOADER2_STAMP =", b"ACTIVE_CIX_RELEASE_BOOTLOADER2_STAMP =")))
            digest = hashlib.sha256(definitions + makefile[start:end] +
                                    show_file(root, ref, "scripts/cix_release_cache.py")).hexdigest()
            variants.setdefault(digest, []).append(ref)
        self.assertTrue(variants, "No retained Unofficial sources")
        for refs in variants.values():
            with self.subTest(refs=refs):
                result = subprocess.run(
                    [sys.executable, "-m", "unittest",
                     "test_cix_release_dependencies.CixReleaseDependencyTests"],
                    cwd=root / "scripts", env={**os.environ, "SOURCE_TEST_REF": refs[0]},
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
