#!/usr/bin/env python3
"""Reviewed three-way preimages and executable 202305 overlay behavior."""
from __future__ import annotations

import ast
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from radxa13_source_conflicts import (
    OVERLAY_INPUTS, OVERLAY_REVIEWS, OVERLAY_PATHS, RadxaSourceConflictError,
    resolve_radxa13_overlay_conflict, validated_overlay_resolution,
)

FIXTURE = Path(__file__).parent / "tests/fixtures/radxa13-202305-overlay"


def inputs():
    return {path: (FIXTURE / (Path(path).name + ".conflict")).read_bytes() for path in OVERLAY_PATHS}


def canonical(raw):
    return re.sub(rb"(?m)^(<<<<<<<|>>>>>>>) [^\n]+$", rb"\1 reviewed", raw)


class TransformTests(unittest.TestCase):
    def test_actual_three_way_preimages_reproduce_every_conflict(self):
        with tempfile.TemporaryDirectory(prefix="radxa13-overlay-three-way-") as tmp:
            for path, expected in inputs().items():
                name = Path(path).name
                files = []
                for role in ("overlay", "old", "new"):
                    raw = (FIXTURE / (name + "." + role)).read_bytes()
                    file = Path(tmp) / role
                    # Match source_porting.normalise_merge_text's comparison
                    # view. The exact imported fixture bytes remain untouched.
                    file.write_bytes(raw.replace(b"\r\n", b"\n").replace(b"\r", b"\n"))
                    files.append(str(file))
                merge = subprocess.run(["git", "merge-file", "-p", *files], capture_output=True)
                self.assertEqual(merge.returncode, 1, merge.stderr.decode())
                self.assertEqual(canonical(merge.stdout), canonical(expected))

    def test_bounds_reject_changed_files_line_endings_and_path_scope(self):
        for path in OVERLAY_PATHS:
            for mutate in (lambda raw: raw + b"\n/* unrelated */\n",
                           lambda raw: raw.replace(b"\n", b"\r\n")):
                row = inputs()
                row[path] = mutate(row[path])
                with self.subTest(path=path), self.assertRaises(RadxaSourceConflictError):
                    resolve_radxa13_overlay_conflict(row)
        for row in ({OVERLAY_PATHS[0]: inputs()[OVERLAY_PATHS[0]]},
                    inputs() | {"unreviewed": b""}):
            with self.assertRaises(RadxaSourceConflictError):
                resolve_radxa13_overlay_conflict(row)

    def test_labels_can_vary_but_whole_files_cannot(self):
        row = {path: re.sub(rb"(?m)^(<<<<<<<|>>>>>>>) [^\n]+$", rb"\1 regenerated", raw)
               for path, raw in inputs().items()}
        self.assertEqual(resolve_radxa13_overlay_conflict(row),
                         resolve_radxa13_overlay_conflict(inputs()))

    def test_pcdbuild_uses_new_make_variable_and_exact_windows_escapes(self):
        output = resolve_radxa13_overlay_conflict(inputs())[OVERLAY_PATHS[0]]
        module = ast.parse(output)
        assignments = [node for node in module.body if isinstance(node, ast.Assign)
                       and any(isinstance(target, ast.Name) and target.id in
                               ("LinuxCFLAGS", "PcdMakefileEnd") for target in node.targets)]
        namespace = {}
        exec(compile(ast.Module(body=assignments, type_ignores=[]), "reviewed-dsc", "exec"), namespace)
        self.assertEqual(namespace["LinuxCFLAGS"],
                         "CFLAGS += -Wno-pointer-to-int-cast -Wno-unused-variable ")
        self.assertEqual(namespace["PcdMakefileEnd"],
                         "\n!INCLUDE $(BASE_TOOLS_PATH)\\Source\\C\\Makefiles\\ms.common\n"
                         "!INCLUDE $(BASE_TOOLS_PATH)\\Source\\C\\Makefiles\\ms.app\n")
        if shutil.which("make"):
            result = subprocess.run(["make", "--no-print-directory", "-f", "-"],
                                    input=("CFLAGS := -DREVIEWED_BASE\n" + namespace["LinuxCFLAGS"] +
                                           "\nall:\n\t@printf '%s\\n' '$(CFLAGS)'\n").encode(),
                                    capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual(result.stdout.strip(),
                             b"-DREVIEWED_BASE -Wno-pointer-to-int-cast -Wno-unused-variable")


@unittest.skipUnless(shutil.which("cc"), "host C compiler unavailable")
class SecureBootSemanticsTests(unittest.TestCase):
    def test_actual_resolved_fetch_mixed_payloads_and_cleanup(self):
        source = resolve_radxa13_overlay_conflict(inputs())[OVERLAY_PATHS[1]].decode()
        source = source[:source.index("/**\n  Enroll a key/certificate")]
        source = re.sub(r"(?m)^#include.*\n", "", source)
        stubs = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STATIC static
#define BOOLEAN int
#define TRUE 1
#define FALSE 0
#define IN
#define OUT
#define CONST const
#define VOID void
#define DEBUG(x) ((void)0)
#define EFI_SUCCESS 0UL
#define EFI_NOT_FOUND 1UL
#define EFI_INVALID_PARAMETER 2UL
#define EFI_OUT_OF_RESOURCES 3UL
#define EFI_ERROR(x) ((x) != EFI_SUCCESS)
#define EFI_SECTION_RAW 0
#define CopyMem memcpy
typedef uint8_t UINT8;
typedef size_t UINTN;
typedef unsigned long EFI_STATUS;
typedef struct { UINT8 bytes[16]; } EFI_GUID;
typedef struct {
  EFI_GUID SignatureType;
  uint32_t SignatureListSize, SignatureHeaderSize, SignatureSize;
} EFI_SIGNATURE_LIST;
typedef struct { VOID *Data; UINTN DataSize; } SECURE_BOOT_CERTIFICATE_INFO;
static int allocations, rsa_allocations, realloc_calls, create_calls;
static int fail_realloc, fail_create;
static const UINT8 *sections[4];
static UINTN section_sizes[4], section_count;
static VOID *Allocate(UINTN size) {
  VOID *p = malloc(size);
  if (p != NULL) ++allocations;
  return p;
}
static VOID FreePool(VOID *p) { if (p != NULL) { --allocations; free(p); } }
static VOID *ReallocatePool(UINTN old_size, UINTN new_size, VOID *old) {
  (void)old_size;
  ++realloc_calls;
  if (realloc_calls == fail_realloc) return NULL;
  VOID *p = realloc(old, new_size);
  if (p != NULL && old == NULL) ++allocations;
  return p;
}
static EFI_STATUS GetSectionFromAnyFv(EFI_GUID *guid, int kind, UINTN index,
                                     VOID **buffer, UINTN *size) {
  (void)guid; (void)kind;
  if (index >= section_count) return EFI_NOT_FOUND;
  *size = section_sizes[index]; *buffer = Allocate(*size);
  if (*buffer == NULL) abort();
  memcpy(*buffer, sections[index], *size);
  return EFI_SUCCESS;
}
static BOOLEAN RsaGetPublicKeyFromX509(VOID *buffer, UINTN size, VOID **rsa) {
  if (size != 4 || memcmp(buffer, "X509", 4)) return FALSE;
  *rsa = malloc(1); if (*rsa == NULL) abort(); ++rsa_allocations; return TRUE;
}
static VOID RsaFree(VOID *rsa) { --rsa_allocations; free(rsa); }
static EFI_STATUS SecureBootCreateDataFromInput(UINTN *size, EFI_SIGNATURE_LIST **out,
                                              UINTN count, SECURE_BOOT_CERTIFICATE_INFO *info) {
  ++create_calls;
  if (fail_create) return EFI_INVALID_PARAMETER;
  if (count != 1 || info->DataSize != 4 || memcmp(info->Data, "X509", 4)) abort();
  *size = sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_GUID) + 1;
  *out = Allocate(*size); if (*out == NULL) abort();
  memset(*out, 0, *size);
  (*out)->SignatureListSize = (uint32_t)*size;
  (*out)->SignatureSize = sizeof(EFI_GUID) + 1;
  ((UINT8 *)*out)[*size - 1] = 0x58;
  return EFI_SUCCESS;
}
'''
        checks = r'''
static int run(const UINT8 **data, UINTN *sizes, UINTN count, EFI_STATUS expected,
               int expected_creates, int expected_reallocs, UINTN expected_size,
               const UINT8 *expected_data) {
  UINTN size = 99;
  EFI_SIGNATURE_LIST *out = (VOID *)(uintptr_t)1;
  EFI_GUID guid = {{0}};
  section_count = count; create_calls = realloc_calls = 0;
  for (UINTN i = 0; i < count; ++i) { sections[i] = data[i]; section_sizes[i] = sizes[i]; }
  EFI_STATUS status = SecureBootFetchData(&guid, &size, &out);
  if (status != expected || create_calls != expected_creates || realloc_calls != expected_reallocs)
    return 1;
  if (expected != EFI_SUCCESS) {
    if (out != NULL || size != 0) return 2;
  } else {
    if (size != expected_size || (expected_data && memcmp(out, expected_data, size))) return 3;
    FreePool(out);
  }
  return allocations != 0 || rsa_allocations != 0 ? 4 : 0;
}
int main(void) {
  UINT8 esl[sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_GUID) + 1] = {0};
  EFI_SIGNATURE_LIST *list = (VOID *)esl;
  list->SignatureListSize = sizeof(esl); list->SignatureSize = sizeof(EFI_GUID) + 1;
  esl[sizeof(esl) - 1] = 0x45;
  const UINT8 *data[] = {(const UINT8 *)"X509", esl, (const UINT8 *)"bad"};
  UINTN sizes[] = {4, sizeof(esl), 3};
  UINT8 mixed[sizeof(esl) * 2]; memcpy(mixed, esl, sizeof(esl));
  mixed[sizeof(esl) - 1] = 0x58; memcpy(mixed + sizeof(esl), esl, sizeof(esl));
  if (run(data, sizes, 0, EFI_NOT_FOUND, 0, 0, 0, NULL)) return 1;
  if (run(data, sizes, 1, EFI_SUCCESS, 1, 1, sizeof(esl), mixed)) return 2;
  if (run(data + 1, sizes + 1, 1, EFI_SUCCESS, 0, 1, sizeof(esl), esl)) return 3;
  if (run(data, sizes, 2, EFI_SUCCESS, 1, 2, sizeof(mixed), mixed)) return 4;
  if (run(data, sizes, 3, EFI_INVALID_PARAMETER, 1, 2, 0, NULL)) return 5;
  if (run(data + 2, sizes + 2, 1, EFI_INVALID_PARAMETER, 0, 0, 0, NULL)) return 6;
  fail_realloc = 1;
  if (run(data, sizes, 1, EFI_OUT_OF_RESOURCES, 1, 1, 0, NULL)) return 7;
  fail_realloc = 2;
  if (run(data, sizes, 2, EFI_OUT_OF_RESOURCES, 1, 2, 0, NULL)) return 8;
  fail_realloc = 0; fail_create = 1;
  if (run(data, sizes, 1, EFI_INVALID_PARAMETER, 1, 0, 0, NULL)) return 9;
  fail_create = 0;
  list->SignatureListSize = sizeof(esl) + 1;
  if (run(data + 1, sizes + 1, 1, EFI_INVALID_PARAMETER, 0, 0, 0, NULL)) return 10;
  list->SignatureListSize = sizeof(esl); list->SignatureSize = sizeof(EFI_GUID) + 2;
  if (run(data + 1, sizes + 1, 1, EFI_INVALID_PARAMETER, 0, 0, 0, NULL)) return 11;
  return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="radxa13-overlay-secureboot-") as tmp:
            root = Path(tmp)
            file = root / "fetch.c"
            file.write_text(stubs + source + checks)
            result = subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", str(file),
                                     "-o", str(root / "test")], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            result = subprocess.run([str(root / "test")], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())


@unittest.skipUnless(os.getenv("RADXA13_OVERLAY_REPO") and os.getenv("RADXA13_OVERLAY_JOURNAL"),
                     "set RADXA13_OVERLAY_REPO and RADXA13_OVERLAY_JOURNAL for complete-tree checks")
class JournalTests(unittest.TestCase):
    def setUp(self):
        self.repo = Path(os.environ["RADXA13_OVERLAY_REPO"])
        self.row = json.loads(Path(os.environ["RADXA13_OVERLAY_JOURNAL"]).read_text())
        self.pair = self.row["pair"]
        self.inputs = OVERLAY_REVIEWS[tuple(self.pair)][0]
        self.tmp = tempfile.TemporaryDirectory(prefix="radxa13-overlay-journal-")
        self.addCleanup(self.tmp.cleanup)
        self.journal = Path(self.tmp.name) / "journal.json"

    def check_row(self, row, **kwargs):
        self.journal.write_text(json.dumps(row))
        return validated_overlay_resolution(
            self.repo, self.journal, kwargs.get("edk2", self.pair[0]), kwargs.get("radxa", self.pair[1]),
            kwargs.get("source_ref", self.inputs["source"][0]),
            kwargs.get("base_ref", self.inputs["new"][0]))

    def test_exact_journal_and_selected_inputs(self):
        self.assertEqual(self.check_row(self.row), self.row["resolution_commit"])
        for kwargs in ({"edk2": "202302"}, {"source_ref": self.inputs["old"][0]},
                       {"base_ref": OVERLAY_INPUTS["old"][0]},
                       {"radxa": "1.3.0" if self.pair[1] == "1.3.1" else "1.3.1"}):
            with self.assertRaises(RadxaSourceConflictError):
                self.check_row(self.row, **kwargs)

    def test_receipt_scope_parent_and_tree_tampering_rejected(self):
        for key, value in (("pair", ["202305", "1.2.1"]), ("stage", "source"),
                           ("paths", list(OVERLAY_PATHS)[1:]), ("conflict_tree", "0" * 40),
                           ("resolution_tree", "0" * 40), ("resolution_ref", "refs/heads/build"),
                           ("resolution_commit", self.row["conflict_commit"]),
                           ("resolution_commit", 42)):
            with self.subTest(key=key), self.assertRaises(RadxaSourceConflictError):
                self.check_row(self.row | {key: value})
        for row in ([], self.row | {"inputs": []}):
            with self.assertRaises(RadxaSourceConflictError):
                self.check_row(row)
        for role in self.inputs:
            for field in ("ref", "commit", "tree"):
                row = json.loads(json.dumps(self.row))
                row["inputs"][role][field] = "0" * 40
                with self.subTest(role=role, field=field), self.assertRaises(RadxaSourceConflictError):
                    self.check_row(row)

    def test_other_reviewed_pair_inputs_cannot_be_substituted(self):
        other = "1.3.0" if self.pair[1] == "1.3.1" else "1.3.1"
        inputs, tree = OVERLAY_REVIEWS[("202305", other)]
        for role, (ref, oid, input_tree) in inputs.items():
            row = json.loads(json.dumps(self.row))
            row["inputs"][role] = {"ref": ref, "commit": oid, "tree": input_tree}
            with self.subTest(role=role), self.assertRaises(RadxaSourceConflictError):
                self.check_row(row)
        with self.assertRaises(RadxaSourceConflictError):
            self.check_row(self.row | {"conflict_tree": tree})

    def test_reviewed_output_with_wrong_file_mode_rejected(self):
        env = os.environ.copy()
        env["GIT_INDEX_FILE"] = str(Path(self.tmp.name) / "mode-index")
        command = ["git", "-C", str(self.repo)]
        subprocess.run(command + ["read-tree", self.row["resolution_commit"]], env=env, check=True)
        blob = subprocess.check_output(command + ["rev-parse", self.row["resolution_commit"] + ":" + OVERLAY_PATHS[0]]).decode().strip()
        subprocess.run(command + ["update-index", "--cacheinfo", "100755," + blob + "," + OVERLAY_PATHS[0]], env=env, check=True)
        tree = subprocess.check_output(command + ["write-tree"], env=env).decode().strip()
        commit = subprocess.check_output(command + ["commit-tree", tree, "-p", self.row["conflict_commit"],
                                                    "-m", "negative-mode-test-only"], env=env).decode().strip()
        with self.assertRaises(RadxaSourceConflictError):
            self.check_row(self.row | {"resolution_commit": commit, "resolution_tree": tree})

    def test_unrelated_tree_entry_and_wrong_parent_rejected(self):
        env = os.environ.copy()
        env["GIT_INDEX_FILE"] = str(Path(self.tmp.name) / "index")
        command = ["git", "-C", str(self.repo)]
        subprocess.run(command + ["read-tree", self.row["resolution_commit"]], env=env, check=True)
        blob = subprocess.check_output(command + ["hash-object", "-w", "--stdin"], input=b"unreviewed\n").decode().strip()
        subprocess.run(command + ["update-index", "--add", "--cacheinfo", "100644," + blob + ",unreviewed.c"], env=env, check=True)
        tree = subprocess.check_output(command + ["write-tree"], env=env).decode().strip()
        for parent in (self.row["conflict_commit"], self.row["resolution_commit"]):
            commit = subprocess.check_output(command + ["commit-tree", tree, "-p", parent,
                                                        "-m", "negative-test-only"], env=env).decode().strip()
            with self.assertRaises(RadxaSourceConflictError):
                self.check_row(self.row | {"resolution_commit": commit, "resolution_tree": tree})


if __name__ == "__main__":
    unittest.main()
