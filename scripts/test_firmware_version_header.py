#!/usr/bin/env python3
"""Compile the real custom version header and inspect it before DXE runs."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from reconstruction_common import show_file

ROOT = Path(__file__).resolve().parents[1]
DRIVER = "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionDxe.c"
HEADER = "src/edk2-platforms/Platform/CIX/Sky1/Include/Protocol/FwVersionProtocol.h"


def source(relative):
    local = os.environ.get("SOURCE_TEST_ROOT")
    if local:
        return (Path(local) / relative).read_text()
    return show_file(ROOT, os.environ.get("SOURCE_TEST_REF", "source/unofficial/1.3/current"), relative).decode()


class FirmwareVersionHeaderTests(unittest.TestCase):
    def test_stored_version_follows_each_build_without_runtime_initialisation(self):
        declaration = re.search(
            r"GLOBAL_REMOVE_IF_UNREFERENCED CIX_FW_VERSION_PROTOCOL\s+CixFwVerProtocol = \{.*?^\};",
            source(DRIVER), re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(declaration)
        with tempfile.TemporaryDirectory(prefix="firmware-version-header-") as tmp:
            root = Path(tmp)
            (root / "Uefi.h").write_text("""
#include <stdint.h>
typedef char CHAR8;
typedef uint16_t CHAR16;
typedef uint8_t UINT8;
typedef uint32_t UINT32;
typedef unsigned long EFI_STATUS;
#define IN
#define OUT
#define EFIAPI
#define GLOBAL_REMOVE_IF_UNREFERENCED
""")
            (root / "FwVersionProtocol.h").write_text(source(HEADER))
            (root / "probe.c").write_text("""
#include <stdio.h>
#include "FwVersionProtocol.h"
EFI_STATUS GetFwVersion(FW_VERSION_TYPE type, CHAR16 **buffer, UINT32 *size) {
    (void)type; (void)buffer; (void)size;
    return 0;
}
""" + declaration.group() + """
int main(void) {
    return fwrite(&CixFwVerProtocol.FwHeader, sizeof(FW_VER_HEADER), 1, stdout) != 1;
}
""")
            compiler = shutil.which("cc") or "cc"
            for version in ("1.3.1+fixes", "1.3.1+cix+fixes+verbose+mask80000040", "1.3.1+fixes"):
                with self.subTest(version=version):
                    subprocess.run([
                        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root),
                        "-DUEFI_FW_VERSION=" + version, str(root / "probe.c"), "-o", str(root / "probe"),
                    ], check=True, capture_output=True, text=True)
                    data = subprocess.check_output([str(root / "probe")])
                    self.assertEqual(len(data), 81)
                    self.assertEqual(data[:16].rstrip(b"\0"), b"$UEFI_FIRMWARE$")
                    self.assertEqual(data[16], 1)
                    self.assertEqual(data[17:].split(b"\0", 1)[0].decode("ascii"), version)


if __name__ == "__main__":
    unittest.main()
