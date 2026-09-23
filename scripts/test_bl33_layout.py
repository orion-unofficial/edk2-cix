#!/usr/bin/env python3
"""Prove BL33 consent, physical bounds, signed payloads and measured resizing."""

import ast
import copy
import json
from pathlib import Path
import pathlib
import sys
import tempfile
import unittest

from bl33_layout import limits, prepare, select
from build_bl33 import build, required_size, resize_fdf
from debug_build_policy import fd_size
from firmware_chain import ChainError
from reconstruction_common import for_each_ref, show_file
from test_firmware_chain import SignedFirmwareFixture, vendor
from validate_firmware_chain import check_flash, check_ota, validate_uefi
from validate_release_inputs import source_fdf
from warn_debug_categories import audit

ROOT = Path(__file__).resolve().parents[1]


class Bl33LayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="adaptive-bl33-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.fixture = SignedFirmwareFixture(cls.root / "package", payload=bytes(0x220000))
        (cls.fixture.package / "spi_flash_config_ota.json").write_bytes(vendor("1.3.1", "spi_flash_config_ota.json"))

    def test_actual_fip_boundary_and_consent(self):
        selected = self.fixture.selected
        self.assertEqual(limits(selected)["maximum_slot_size"], 0x3FA000)
        self.assertEqual(select(selected, 0x1F9000, True)["mode"], "original")
        with self.assertRaisesRegex(ChainError, "DEBUG_ALLOW_LARGE_IMAGE=1"):
            select(selected, 0x1F9001)
        self.assertEqual(select(selected, 0x3FA000, True)["slot_size"], 0x3FA000)
        with self.assertRaisesRegex(ChainError, "maximum supported"):
            select(selected, 0x3FA001, True)

    def test_fd_runtime_memory_bound_is_separate_from_flash_capacity(self):
        selected = copy.deepcopy(self.fixture.selected)
        selected["flash_layout"]["entries"]["7"] = {"address": 0x3FE000, "size": 0x402000}
        self.assertEqual(limits(selected)["maximum_slot_size"], 0x402000)
        self.assertEqual(limits(selected)["maximum_fd_size"], 0x400000)
        with tempfile.TemporaryDirectory(prefix="bl33-ram-") as directory:
            oversized = SignedFirmwareFixture(Path(directory), payload=bytes(0x400001))
            with self.assertRaisesRegex(ChainError, "reserved RAM"):
                validate_uefi(oversized.uefi, oversized.selected)

    def test_other_loaders_and_later_allocations_cannot_inherit_extension(self):
        selected = copy.deepcopy(self.fixture.selected)
        selected["bl1_sha256"] = "0" * 64
        with self.assertRaisesRegex(ChainError, "maximum supported"):
            select(selected, 0x200000, True)
        selected = copy.deepcopy(self.fixture.selected)
        selected["flash_layout"]["entries"]["99"] = {"address": 0x700000, "size": 0x1000}
        with self.assertRaisesRegex(ChainError, "overlap"):
            select(selected, 0x200000, True)

    def test_enlargement_keeps_chain_checks_and_requires_full_image(self):
        image, end = self.fixture.flash_fixture()
        image[end:] = b"\xff" * (len(image) - end)
        with self.assertRaisesRegex(ChainError, "reserved size"):
            check_flash(image, self.fixture.selected)
        selected = {**self.fixture.selected, "allow_large_bl33": True}
        result = check_flash(image, selected)
        self.assertEqual(result["bl33_layout"]["mode"], "full-image")
        with self.assertRaisesRegex(ChainError, "reserved size"):
            check_ota(self.fixture.ota_fixture(), selected)
        image[end - 1] ^= 1
        with self.assertRaisesRegex(ChainError, "digest"):
            check_flash(image, selected)

    def test_prepare_changes_only_bl33_slot_and_preserves_source_files(self):
        package = self.fixture.package
        before = {p.name: p.read_bytes() for p in package.glob("spi_flash_config_*.json")}
        with tempfile.TemporaryDirectory(prefix="bl33-stage-") as directory:
            stage = Path(directory)
            (stage / "Firmwares").mkdir()
            (stage / "Firmwares/bootloader3.img").write_bytes(self.fixture.uefi)
            result = prepare(stage, package, True)
            self.assertTrue(result["full_image_only"])
            self.assertTrue((stage / "bl33-full-image-only").is_file())
            for name, original in before.items():
                expected = json.loads(original)
                for row in expected["image_header_groups"]:
                    if row["image_type"] == 7:
                        row["size"] = "0x3fa000"
                self.assertEqual(json.loads((stage / name).read_text()), expected)
                self.assertEqual((package / name).read_bytes(), original)
            self.assertEqual(prepare(stage, package, True), result)
            with self.assertRaisesRegex(ChainError, "DEBUG_ALLOW_LARGE_IMAGE"):
                prepare(stage, package, False)
            config = json.loads((stage / "spi_flash_config_all.json").read_text())
            config["image_header_groups"][0]["size"] = "0x1"
            (stage / "spi_flash_config_all.json").write_text(json.dumps(config))
            with self.assertRaisesRegex(ChainError, "more than"):
                prepare(stage, package, True)

    def test_small_image_after_large_image_restores_original_config_and_ota(self):
        with tempfile.TemporaryDirectory(prefix="bl33-small-") as directory:
            root = Path(directory)
            small = SignedFirmwareFixture(root / "package")
            (small.package / "spi_flash_config_ota.json").write_bytes(vendor("1.3.1", "spi_flash_config_ota.json"))
            stage = root / "stage"
            (stage / "Firmwares").mkdir(parents=True)
            payload = stage / "Firmwares/bootloader3.img"
            payload.write_bytes(self.fixture.uefi)
            self.assertTrue(prepare(stage, small.package, True)["full_image_only"])
            payload.write_bytes(small.uefi)
            self.assertEqual(prepare(stage, small.package, True)["mode"], "original")
            self.assertFalse((stage / "bl33-full-image-only").exists())
            for kind in ("all", "ota"):
                name = f"spi_flash_config_{kind}.json"
                self.assertEqual((stage / name).read_bytes(), (small.package / name).read_bytes())

    def test_source_fdf_resizing_for_every_retained_board_and_target(self):
        seen = set()
        for ref in for_each_ref(ROOT, "source/unofficial/"):
            for board in ("O6", "O6N"):
                text = source_fdf(ROOT, ref, board)
                if text in seen:
                    continue
                seen.add(text)
                changed = resize_fdf(text, 0x234000)
                for target in ("RELEASE", "DEBUG"):
                    self.assertEqual(fd_size(changed, target), 0x234000, ref)
                self.assertIn("BaseAddress   = 0x84400000", changed)

    def test_only_an_identified_outer_fv_overflow_can_trigger_retry(self):
        text = "FVMAIN_COMPACT.inf\nthe required fv image size 0x201234 exceeds the set fv image size 0x200000\n"
        self.assertEqual(required_size(text, 0x200000), 0x202000)
        for broken in (text.replace("FVMAIN_COMPACT", "OTHER"), text + "file.c:4: error: broken\n",
                       text + "error F002: Failed to build module\n", text + text):
            self.assertIsNone(required_size(broken, 0x200000))
        self.assertIsNone(required_size(text, 0x100000))

    def test_measured_retry_uses_private_fdf_and_preserves_original(self):
        with tempfile.TemporaryDirectory(prefix="bl33-resize-") as directory:
            root = Path(directory)
            fdf = root / "O6.fdf"
            text = source_fdf(ROOT, "source/unofficial/1.3.1/edk2-stable202608", "O6")
            fdf.write_text(text)
            fake = root / "build.py"
            fake.write_text('''import sys
if '-f' not in sys.argv:
 print('FVMAIN_COMPACT.inf')
 print('the required fv image size 0x201234 exceeds the set fv image size 0x1f4000')
 sys.exit(2)
from pathlib import Path
assert '0x202000' in Path(sys.argv[sys.argv.index('-f')+1]).read_text()
print('built')
''')
            self.assertEqual(build([sys.executable, str(fake)], fdf, "RELEASE", root, 0x3FA000), 0)
            self.assertEqual(fdf.read_text(), text)
            attempts = json.loads((root / "bl33-sizing/attempts.json").read_text())
            self.assertEqual([row["fd_size"] for row in attempts], [0x1F4000, 0x202000])
            with self.assertRaisesRegex(ValueError, "supported flash space"):
                build([sys.executable, str(fake)], fdf, "RELEASE", root, 0x200000)

    def test_dependency_audit_preserves_dynamic_and_all_level_caveat(self):
        with tempfile.TemporaryDirectory(prefix="debug-sites-") as directory:
            root = Path(directory)
            header = root / "DebugLib.h"
            header.write_text('#define DEBUG_ERROR 0x80000000\n#define DEBUG_BM 0x400\n#define DEBUG_FS 0x8\n')
            source = root / "file.c"
            source.write_text('#define PROGRESS DEBUG_BM\nDEBUG ((PROGRESS, "boot"));\nDebugPrint (-1, "all");\n')
            (root / "file.obj.deps").write_text(f'file.obj: {source} {header}\n')
            report = audit(root, header, 0x408)
            self.assertEqual(report["categories_without_labelled_sites"], ["DEBUG_FS"])
            self.assertIn("Dynamic/all-level", report["limits"])
            source.unlink()
            self.assertFalse(audit(root, header, 0x408)["categories_without_labelled_sites"])

    def test_real_export_mapping_omits_only_enlarged_custom_ota(self):
        source = show_file(ROOT, 'source/unofficial/1.3.1/edk2-stable202608',
                           'scripts/export_firmware_payload.py').decode()
        function = next(node for node in ast.parse(source).body
                        if isinstance(node, ast.FunctionDef) and node.name == 'payload_mapping')
        scope = {'pathlib': pathlib, 'json': json, 'should_stage_load_op_rom': lambda *_: False}
        exec(compile(ast.Module(body=[function], type_ignores=[]), 'export_firmware_payload.py', 'exec'), scope)
        with tempfile.TemporaryDirectory(prefix='bl33-export-') as directory:
            root = Path(directory)
            output = root / 'src/Build/O6/RELEASE_GCC'
            output.mkdir(parents=True)
            for enlarged in (False, True, False):
                (output / 'bl33-layout.json').write_text(json.dumps({'full_image_only': enlarged}))
                for mode in ('upstream', 'custom'):
                    names = {dest.name for _, dest in scope['payload_mapping'](root, 'O6', 'RELEASE_GCC', mode)}
                    self.assertEqual('cix_flash_ota.bin' in names, mode == 'upstream' or not enlarged)
                    self.assertEqual('bl33-layout.json' in names, mode == 'custom')


if __name__ == "__main__":
    unittest.main()
