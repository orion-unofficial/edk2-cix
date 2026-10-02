#!/usr/bin/env python3
"""Whole-source bounds, capsule linkage and executable reset-flow regressions."""
from __future__ import annotations
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from radxa13_source_conflicts import (
    CAPSULE, INPUTS, PATHS, RESET, RadxaSourceConflictError,
    resolve_radxa13_source_conflict, validated_source_resolution,
)

FIXTURE = Path(__file__).parent / "tests/fixtures/radxa13-source-conflicts"


def inputs():
    return {p: (FIXTURE / Path(p).name).read_bytes() for p in PATHS}


class TransformTests(unittest.TestCase):
    def test_reviewed_output_and_scope(self):
        output = resolve_radxa13_source_conflict(inputs())
        self.assertIsNone(output[RESET + "ArmPsciResetSystemLib.c"])
        self.assertEqual(set(output), set(PATHS))
        self.assertTrue(all(b"<<<<<<<" not in raw for raw in output.values() if raw))

    def test_every_whole_file_change_rejected(self):
        for path in PATHS:
            with self.subTest(path=path):
                row = inputs()
                row[path] += b"\n/* unrelated change */\n"
                with self.assertRaises(RadxaSourceConflictError):
                    resolve_radxa13_source_conflict(row)

    def test_missing_or_additional_path_rejected(self):
        for row in ({p: v for p, v in inputs().items() if p != PATHS[0]},
                    inputs() | {"unreviewed.c": b""}):
            with self.assertRaises(RadxaSourceConflictError):
                resolve_radxa13_source_conflict(row)

    def test_commit_label_variation_keeps_reviewed_bytes(self):
        row = {p: re.sub(rb"(?m)^(<<<<<<<|>>>>>>>) [^\n]+$", rb"\1 new-commit", raw)
               for p, raw in inputs().items()}
        self.assertEqual(resolve_radxa13_source_conflict(row), resolve_radxa13_source_conflict(inputs()))

    def test_wrong_conflict_side_and_line_endings_rejected(self):
        for old, new in ((b"extern BOOLEAN", b"extern UINTN"), (b"\n", b"\r\n")):
            row = inputs()
            row[CAPSULE + "DxeCapsuleLib.c"] = row[CAPSULE + "DxeCapsuleLib.c"].replace(old, new)
            with self.assertRaises(RadxaSourceConflictError):
                resolve_radxa13_source_conflict(row)


@unittest.skipUnless(shutil.which("cc"), "host C compiler unavailable")
class SourceSemanticsTests(unittest.TestCase):
    def compile_run(self, sources):
        with tempfile.TemporaryDirectory(prefix="radxa13-semantics-") as tmp:
            root = Path(tmp)
            files = []
            for name, body in sources.items():
                file = root / name
                file.write_text(body)
                files.append(str(file))
            result = subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", *files, "-o", str(root / "test")], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            subprocess.run([str(root / "test")], check=True, capture_output=True)

    def test_both_capsule_inf_variants_link_one_global(self):
        output = resolve_radxa13_source_conflict(inputs())
        common = output[CAPSULE + "DxeCapsuleLib.c"].decode()
        runtime = output[CAPSULE + "DxeCapsuleRuntime.c"].decode()
        declaration = re.compile(r"(?m)^(?:extern )?BOOLEAN\s+mDxeCapsuleLibReadyToBootEvent[^\n]*$")
        for inf in ("DxeCapsuleLib.inf", "DxeRuntimeCapsuleLib.inf"):
            body = (FIXTURE / inf).read_text()
            section = re.search(r"\[Sources\]\n(.*?)(?=\n\[)", body, re.S)[1]
            # These source lists have no build-condition branches.
            self.assertNotIn("!if", section.lower())
            self.assertIn("  DxeCapsuleLib.c\n", section)
            selected = {"common.c": "typedef int BOOLEAN;\n#define FALSE 0\n" + declaration.search(common)[0] + "\nint read_common(void) { return mDxeCapsuleLibReadyToBootEvent; }\n"}
            if inf == "DxeRuntimeCapsuleLib.inf":
                self.assertIn("  DxeCapsuleRuntime.c\n", section)
                selected["runtime.c"] = "typedef int BOOLEAN;\n" + declaration.search(runtime)[0] + "\nvoid set_ready(void) { mDxeCapsuleLibReadyToBootEvent = 1; }\n"
                main = "void set_ready(void); int read_common(void); int main(void) { if (read_common()) return 1; set_ready(); return read_common() != 1; }\n"
            else:
                self.assertNotIn("DxeCapsuleRuntime.c", section)
                main = "int read_common(void); int main(void) { return read_common(); }\n"
            selected["main.c"] = main
            with self.subTest(inf=inf):
                self.compile_run(selected)

    def test_standard_reset_entry_points_and_vendor_fallback(self):
        output = resolve_radxa13_source_conflict(inputs())
        source = output[RESET + "ResetSystemLib.c"].decode()
        source = re.sub(r"(?m)^#include.*\n", "", source)
        inf = (FIXTURE / "ResetSystemLib.inf").read_text()
        self.assertIn("[Sources]\n  ResetSystemLib.c", inf)
        self.assertNotIn("ArmPsciResetSystemLib.c", inf)
        self.assertIn("ResetSystemLib|Platform/CIX/Sky1/Library/ArmPsciResetSystemLib/ResetSystemLib.inf",
                      (FIXTURE / "Sky1Common.dsc.inc").read_text())
        stubs = r'''
#include <setjmp.h>
#include <stdlib.h>
#define STATIC static
#define VOID void
#define IN
#define OPTIONAL
#define FALSE 0
#define EFIAPI
#define DEBUG(x) ((void)0)
#define ASSERT(x) do { if (!(x)) ++asserts; } while (0)
#define EFI_ERROR(x) (((x) & ((EFI_STATUS)1 << (sizeof(EFI_STATUS)*8 - 1))) != 0)
#define ARM_SMC_ID_PSCI_SYSTEM_RESET 0x84000009UL
#define ARM_SMC_ID_PSCI_SYSTEM_OFF 0x84000008UL
#define FixedPcdGet8(x) 0
#define OUTPUT 0
#define INOUT_LOW 0
#define INTERRUPT_DISABLE 0
#define INTERRUPT_TYPE_DEFAULT 0
typedef unsigned long UINTN;
typedef unsigned long EFI_STATUS;
typedef enum { EfiResetWarm, EfiResetCold, EfiResetShutdown, EfiResetPlatformSpecific } EFI_RESET_TYPE;
typedef struct { UINTN Arg0; } ARM_SMC_ARGS;
typedef struct { int Reserved; } EC_PARAMS_FORCE_EC_RESET;
static jmp_buf end;
static EFI_STATUS ec_status;
static int ec_calls, smc_calls, gpio_calls, waits, asserts;
static UINTN last_smc;
static EFI_STATUS ForceEcReset(EC_PARAMS_FORCE_EC_RESET *p) { if (p->Reserved) abort(); ++ec_calls; return ec_status; }
static void ArmCallSmc(ARM_SMC_ARGS *p) { ++smc_calls; last_smc=p->Arg0; }
static void GpioConfig(int a,int b,int c,int d,int e) { ++gpio_calls; }
static void CpuDeadLoop(void) { ++waits; longjmp(end, 1); }
'''
        checks = r'''
static int run(EFI_RESET_TYPE type, EFI_STATUS status) {
  ec_status=status; ec_calls=smc_calls=gpio_calls=waits=asserts=0; last_smc=0;
  if (!setjmp(end)) ResetSystem(type, 0, 0, 0);
  return 0;
}
int main(void) {
  run(EfiResetPlatformSpecific, 0);
  if (ec_calls!=1 || smc_calls || gpio_calls || waits!=1) return 1;
  run(EfiResetPlatformSpecific, (EFI_STATUS)1 << (sizeof(EFI_STATUS)*8 - 1));
  if (ec_calls!=1 || smc_calls!=1 || last_smc!=ARM_SMC_ID_PSCI_SYSTEM_RESET || waits!=1) return 2;
  run(EfiResetCold, 0);
  if (ec_calls || smc_calls!=1 || last_smc!=ARM_SMC_ID_PSCI_SYSTEM_RESET || waits!=1) return 3;
  run(EfiResetWarm, 0);
  if (ec_calls || smc_calls!=1 || last_smc!=ARM_SMC_ID_PSCI_SYSTEM_RESET || waits!=1) return 4;
  run(EfiResetShutdown, 0);
  if (ec_calls || gpio_calls!=5 || smc_calls!=1 || last_smc!=ARM_SMC_ID_PSCI_SYSTEM_OFF || waits!=1) return 5;
  run(EfiResetPlatformSpecific, 1); /* UEFI warning is not an error. */
  if (ec_calls!=1 || smc_calls || gpio_calls || waits!=1) return 7;
  run((EFI_RESET_TYPE)999, 0);
  if (ec_calls || gpio_calls || smc_calls || waits || asserts!=1) return 6;
  return 0;
}
'''
        self.compile_run({"reset.c": stubs + source + checks})


# The exact complete-tree journal checks run against the isolated reviewed
# repository when requested. Unit source tests never require a firmware clone.
@unittest.skipUnless(os.getenv("RADXA13_REVIEW_REPO") and os.getenv("RADXA13_REVIEW_JOURNAL"),
                     "set RADXA13_REVIEW_REPO and RADXA13_REVIEW_JOURNAL for complete-tree guard checks")
class JournalTests(unittest.TestCase):
    def setUp(self):
        self.repo = Path(os.environ["RADXA13_REVIEW_REPO"])
        self.row = json.loads(Path(os.environ["RADXA13_REVIEW_JOURNAL"]).read_text())
        self.tmp = tempfile.TemporaryDirectory(prefix="radxa13-journal-")
        self.addCleanup(self.tmp.cleanup)
        self.journal = Path(self.tmp.name) / "journal.json"

    def check_row(self, row):
        self.journal.write_text(json.dumps(row))
        return validated_source_resolution(self.repo, self.journal, "202408.01", "1.3.1")

    def test_exact_resolution(self):
        self.assertEqual(self.check_row(self.row), self.row["resolution_commit"])

    def test_pair_scope_receipt_and_ref_tampering_rejected(self):
        for key, value in (("pair", ["202408", "1.3.1"]), ("stage", "overlay"),
                           ("paths", list(PATHS)[1:]), ("resolution_tree", "0" * 40),
                           ("resolution_ref", "refs/heads/build"),
                           ("resolution_commit", self.row["conflict_commit"])):
            with self.subTest(key=key):
                with self.assertRaises(RadxaSourceConflictError):
                    self.check_row(self.row | {key: value})

    def test_malformed_receipt_structure_rejected(self):
        for row in ([], self.row | {"inputs": []}, self.row | {"inputs": {"old": []}},
                    self.row | {"resolution_commit": 42}):
            with self.subTest(row=row):
                with self.assertRaises(RadxaSourceConflictError):
                    self.check_row(row)

    def test_original_and_normalized_input_tampering_rejected(self):
        for role in INPUTS:
            for key in ("original_commit", "normalized_commit", "normalized_tree"):
                with self.subTest(role=role, key=key):
                    row = json.loads(json.dumps(self.row))
                    row["inputs"][role][key] = "0" * 40
                    with self.assertRaises(RadxaSourceConflictError):
                        self.check_row(row)

    def test_unrelated_posttree_change_rejected(self):
        # Use a separate index and an unreferenced test commit. No checkout or
        # canonical/private candidate ref is changed by this negative test.
        env = os.environ.copy()
        env["GIT_INDEX_FILE"] = str(Path(self.tmp.name) / "index")
        command = ["git", "-C", str(self.repo)]
        subprocess.run(command + ["read-tree", self.row["resolution_commit"]], env=env, check=True)
        blob = subprocess.check_output(command + ["hash-object", "-w", "--stdin"], input=b"unreviewed\n").decode().strip()
        subprocess.run(command + ["update-index", "--add", "--cacheinfo", "100644," + blob + ",unreviewed-source.c"], env=env, check=True)
        tree = subprocess.check_output(command + ["write-tree"], env=env).decode().strip()
        commit = subprocess.check_output(command + ["commit-tree", tree, "-p", self.row["conflict_commit"], "-m", "negative-test-only"], env=env).decode().strip()
        with self.assertRaisesRegex(RadxaSourceConflictError, "outside the reviewed"):
            self.check_row(self.row | {"resolution_commit": commit, "resolution_tree": tree})


if __name__ == "__main__":
    unittest.main()
