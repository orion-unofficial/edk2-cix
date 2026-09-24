#!/usr/bin/env python3
"""Regression coverage for sibling modules hidden by partial overlays."""

import os
import ast
import warnings
import posixpath
import subprocess
from pathlib import Path
import tempfile
import unittest

from check_source_build_inputs import BUILD_FIXES, chain_validator_problems, missing_board_table_inputs, missing_build_fixes, missing_configuration_manager_types, missing_lto_library, missing_module_infs, missing_package_declarations, missing_platform_inputs, missing_smbios_cache_types, missing_toolchain, missing_wrapper_dependencies, source_input_problems, unbalanced_asl_conditionals
from check_source_build_inputs import autogen_library_problems, missing_tool_definitions, missing_acpi_headers, flattened_overlay_mirrors
from validate_firmware_chain import CIX_KEY_SHA256, CIX_SIGNING_KEYS
from test_support import commit_all, git, write_file
from source_lifecycle import tree_entries
from source_porting import git_blob_bytes_batch
from reconstruction_common import show_file


class SourceBuildInputsTests(unittest.TestCase):
    def test_chain_validator_variants_are_pinned_to_signing_capability(self):
        repo = Path(__file__).resolve().parents[1]
        current = (repo / "scripts/validate_firmware_chain.py").read_bytes()
        stock = show_file(repo, "source/unofficial/1.2.4/edk2-stable202608",
                          "src/scripts/validate_firmware_chain.py")
        self.assertEqual(chain_validator_problems(stock, {}, b"", current), [])
        self.assertTrue(chain_validator_problems(stock + b"# drift\n", {}, b"", current))
        ref = "source/unofficial/1.3.1/edk2-stable202608"
        keys = {name: show_file(repo, ref, f"{CIX_SIGNING_KEYS}/{name}")
                for name in CIX_KEY_SHA256}
        helper = show_file(repo, ref, "src/scripts/build_cix_release_bootloader2.sh")
        self.assertEqual(chain_validator_problems(current, keys, helper, current), [])
        self.assertTrue(chain_validator_problems(stock, keys, helper, current))
        self.assertTrue(chain_validator_problems(current, keys, helper + b"# drift\n", current))
        keys["cix_privatekey.pem"] += b"tampered"
        self.assertTrue(chain_validator_problems(current, keys, helper, current))

    def test_regular_overlay_cannot_contain_a_symlink_blob_as_source(self):
        path = 'custom/overlay/Spcr.aslc'
        self.assertIn(path, flattened_overlay_mirrors({path: b'../../src/Spcr.aslc'})[0])
        self.assertEqual(flattened_overlay_mirrors({path: b'#include <IndustryStandard/Acpi.h>\n'}), [])

    def test_124_202208_acpi_tables_retain_vendor_bytes_and_equivalent_macros(self):
        repo = Path(__file__).resolve().parents[1]
        old = 'source/unofficial/1.2.4/edk2-stable202208'
        new = 'source/unofficial/1.2.4/edk2-stable202608'

        def read(ref, path):
            for _ in range(8):
                entry = tree_entries(repo, ref, (path,))[path]
                text = show_file(repo, ref, path).decode().replace('\r\n', '\n')
                if entry.mode != '120000':
                    return text
                path = posixpath.normpath(posixpath.join(posixpath.dirname(path), text))
            self.fail('unresolvable ACPI table mirror')

        for name in ('Fadt', 'Dbg2', 'Spcr'):
            with self.subTest(table=name):
                path = f'edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/{name}.aslc'
                self.assertEqual(show_file(repo, old, 'src/' + path),
                                 show_file(repo, 'source/vendor/radxa/1.2.4/edk2-stable202208', 'src/' + path))
                overlay = 'custom/overlay/' + path
                self.assertEqual(tree_entries(repo, old, (overlay,))[overlay].mode,
                                 tree_entries(repo, new, (overlay,))[overlay].mode)
                modern = read(new, overlay)
                expected = (modern.replace('#include <AcpiHelperMacros.h>\n', '')
                            .replace('ACPI_NULL_GAS', 'NULL_GAS').replace('ACPI_GAS32', 'ARM_GAS32'))
                self.assertEqual(read(old, overlay), expected)
        old_header = show_file(repo, old, 'src/edk2/EmbeddedPkg/Include/Library/AcpiLib.h').decode()
        new_header = show_file(repo, new, 'src/edk2/MdeModulePkg/Include/AcpiHelperMacros.h').decode()
        for before, after in (('NULL_GAS', 'ACPI_NULL_GAS'), ('ARM_GAS32', 'ACPI_GAS32')):
            old_macro = next(line.strip() for line in old_header.splitlines() if line.startswith('#define ' + before))
            new_macro = next(line.strip() for line in new_header.splitlines() if line.startswith('#define ' + after))
            self.assertEqual(old_macro, new_macro.replace(after, before))

    def test_202208_console_backport_only_adapts_the_fdt_interface(self):
        repo = Path(__file__).resolve().parents[1]
        path = 'custom/overlay-experimental-uefi-settings/edk2/EmbeddedPkg/Drivers/ConsolePrefDxe/ConsolePrefDxe.c'
        for radxa in ('1.2.4', '1.3.1'):
            with self.subTest(radxa=radxa):
                old = f'source/unofficial/{radxa}/edk2-stable202208'
                new = f'source/unofficial/{radxa}/edk2-stable202608'
                expected = show_file(repo, new, path).decode()
                for before, after in (('<Library/FdtLib.h>', '<libfdt.h>'),
                                      ('FdtPathOffset', 'fdt_path_offset'),
                                      ('FdtDelProp', 'fdt_delprop'), ('FdtStrerror', 'fdt_strerror')):
                    expected = expected.replace(before, after)
                self.assertEqual(show_file(repo, old, path).decode(), expected)
                header = show_file(repo, old, 'src/edk2/EmbeddedPkg/Include/libfdt.h').decode()
                for function in ('fdt_path_offset', 'fdt_delprop', 'fdt_strerror'):
                    self.assertRegex(header, function + r'\s*\(')

    def test_primary_202208_acpi_preflight_accepts_only_declared_custom_files(self):
        repo = Path(__file__).resolve().parents[1]
        directories = (
            'edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables',
            'edk2-platforms/Platform/Radxa/Orion/O6/Drivers/AcpiPlatfomTables',
            'edk2-platforms/Platform/Radxa/Orion/O6/Drivers/LinuxAcpiConfig.h',
            'edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/LinuxAcpiConfig.h',
        )
        helper = 'scripts/check_custom_acpi_overlays.py'
        paths = (helper,) + tuple(prefix + path for prefix in ('src/', 'custom/overlay/')
                                  for path in directories)
        for radxa in ('1.2.4', '1.3.1'):
            ref = f'source/unofficial/{radxa}/edk2-stable202208'
            entries = tree_entries(repo, ref, paths)
            blobs = git_blob_bytes_batch(repo, (entry.object_id for entry in entries.values()))
            with self.subTest(radxa=radxa), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                for path, entry in entries.items():
                    target = root / path
                    target.parent.mkdir(parents=True, exist_ok=True)
                    content = blobs[entry.object_id]
                    if entry.mode == '120000':
                        target.symlink_to(content.decode())
                    else:
                        target.write_bytes(content)
                command = [os.sys.executable, str(root / helper), '--repo-root', str(root)]
                result = subprocess.run(command, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                unexpected = root / 'custom/overlay' / directories[0] / 'Unexpected.h'
                unexpected.write_text('/* not a declared custom addition */\n')
                result = subprocess.run(command, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('no imported counterpart', result.stdout)
                self.assertIn('Unexpected.h', result.stdout)

    def test_primary_202208_mpam_backport_keeps_current_table_contents(self):
        repo = Path(__file__).resolve().parents[1]
        directory = 'custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/'

        def read(ref, path):
            for _ in range(8):
                entry = tree_entries(repo, ref, (path,))[path]
                text = show_file(repo, ref, path).decode().replace('\r\n', '\n')
                if entry.mode != '120000':
                    return text
                path = posixpath.normpath(posixpath.join(posixpath.dirname(path), text))
            self.fail('unresolvable MPAM table mirror')

        for radxa in ('1.2.4', '1.3.1'):
            old = f'source/unofficial/{radxa}/edk2-stable202208'
            new = f'source/unofficial/{radxa}/edk2-stable202608'
            with self.subTest(radxa=radxa):
                compatibility = read(old, directory + 'Mpam.aslc')
                compatibility = compatibility.replace(
                    '#include <IndustryStandard/Acpi.h>\n#include "MpamCompat.h"',
                    '#include <IndustryStandard/Acpi65.h>\n#include <IndustryStandard/Mpam.h>',
                ).replace("SIGNATURE_32 ('M', 'P', 'A', 'M')",
                          'EFI_ACPI_MEMORY_SYSTEM_RESOURCE_PARTITIONING_AND_MONITORING_TABLE_SIGNATURE')
                self.assertEqual(compatibility, read(new, directory + 'Mpam.aslc'))
                self.assertEqual(read(old, directory + 'MpamCompat.h'),
                                 read(new, 'src/edk2/MdePkg/Include/IndustryStandard/Mpam.h'))

    def test_acpi_tables_cannot_require_headers_missing_from_older_edk2(self):
        tables = {'custom/overlay/Mpam.aslc': '#include <IndustryStandard/Acpi65.h>\n#include <IndustryStandard/Mpam.h>\n'}
        paths = {'src/edk2/MdePkg/Include/IndustryStandard/Acpi64.h'}
        self.assertEqual(len(missing_acpi_headers(paths, tables)), 2)
        paths.update({'src/edk2/MdePkg/Include/IndustryStandard/Acpi65.h',
                      'src/edk2/MdePkg/Include/IndustryStandard/Mpam.h'})
        self.assertEqual(missing_acpi_headers(paths, tables), [])
        tables['custom/overlay/Fadt.aslc'] = '#include <AcpiHelperMacros.h>\n#include <Include/AcpiPlatform.h>\n'
        paths.add('custom/overlay/Include/AcpiPlatform.h')
        self.assertEqual(missing_acpi_headers(paths, tables),
                         ['custom/overlay/Fadt.aslc: missing ACPI table header AcpiHelperMacros.h'])

    def test_202208_python_warning_repairs_preserve_imported_behavior(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        refs = ([os.environ['SOURCE_PYTHON_TEST_REF']] if os.environ.get('SOURCE_PYTHON_TEST_REF') else
                [f'source/unofficial/{radxa}/edk2-stable202208' for radxa in ('1.2.4', '1.3.1')])
        for ref in refs:
            prefix = 'custom/overlay/edk2/BaseTools/Source/Python/'
            entries = tree_entries(repo, ref, ('src/edk2/BaseTools/Source/Python', prefix))
            overlays = [path for path in entries if path.startswith(prefix) and path.endswith('.py')]
            self.assertTrue(overlays, 'the 202208 baseline requires custom Python compatibility overlays')
            paths = overlays + [path.replace('custom/overlay/', 'src/', 1) for path in overlays]
            blobs = git_blob_bytes_batch(repo, (entries[path].object_id for path in paths))
            for path in overlays:
                with self.subTest(path=path):
                    imported = blobs[entries[path.replace('custom/overlay/', 'src/', 1)].object_id]
                    custom = blobs[entries[path].object_id]
                    with warnings.catch_warnings():
                        warnings.simplefilter('ignore')
                        before = ast.dump(ast.parse(imported), include_attributes=False)
                    with warnings.catch_warnings():
                        warnings.simplefilter('error')
                        after = ast.dump(ast.parse(custom), include_attributes=False)
                    self.assertEqual(before, after)

    def test_toolchain_specific_overrides_keep_generic_preprocessor_macros(self) -> None:
        definitions = b"DEFINE GCC5_AARCH64_CC_FLAGS = flags\r\nDEFINE GCC_VFRPP_FLAGS = flags\r\n"
        valid = b"RELEASE_GCC5_AARCH64_CC_FLAGS = DEF(GCC5_AARCH64_CC_FLAGS)\nRELEASE_GCC5_AARCH64_VFRPP_FLAGS = DEF(GCC_VFRPP_FLAGS)"
        self.assertEqual(missing_tool_definitions(valid, definitions), [])
        errors = missing_tool_definitions(valid.replace(b"DEF(GCC_VFRPP_FLAGS)", b"DEF(GCC5_VFRPP_FLAGS)"), definitions)
        self.assertEqual(len(errors), 1)
        self.assertIn("GCC5_VFRPP_FLAGS", errors[0])

    def test_autogen_repair_preserves_constructor_and_selected_library_sources(self) -> None:
        imported = b"LIBRARY_CLASS = NULL\r\nCONSTRUCTOR = LzmaDecompressLibConstructor\r\n[Sources]\r\nLzma.c\r\n"
        custom = imported.replace(b"NULL", b"LzmaDecompressLib")
        self.assertEqual(autogen_library_problems(imported, custom, b""), [])
        for broken in (custom.replace(b"CONSTRUCTOR", b"#CONSTRUCTOR_REMOVED"),
                       custom.replace(b"Lzma.c", b"WrongSdk.c")):
            self.assertTrue(autogen_library_problems(imported, broken, b""))
        self.assertTrue(autogen_library_problems(imported, custom, b"DpuDxe|Vendor/Driver.inf"))

    def test_included_custom_source_keeps_its_dependencies_in_experimental_wrapper(self) -> None:
        infs = {"custom/overlay/Hook/Hook.inf": "[Guids]\ngEfiEventReadyToBootGuid\n",
                "custom/experimental/Hook/Hook.inf": "[Guids]\n"}
        sources = {"custom/experimental/Hook/Wrapper.c": '#include "../../overlay/Hook/Hook.c"'}
        self.assertEqual(len(missing_wrapper_dependencies(infs, sources)), 1)
        infs["custom/experimental/Hook/Hook.inf"] += "gEfiEventReadyToBootGuid\n"
        self.assertEqual(missing_wrapper_dependencies(infs, sources), [])

    def test_imported_setup_pcds_are_present_in_experimental_ui(self) -> None:
        module = "edk2-platforms/Platform/Board/PlatformConfigDxe/PlatformConfigDxe.inf"
        infs = {"src/" + module: "[FixedPcd]\ngCixTokenSpaceGuid.PcdSPEEn\n",
                "custom/overlay-experimental-uefi-settings/" + module: "[FixedPcd]\n"}
        self.assertEqual(len(missing_wrapper_dependencies(infs, {})), 1)
        infs["custom/overlay-experimental-uefi-settings/" + module] += "gCixTokenSpaceGuid.PcdSPEEn\n"
        self.assertEqual(missing_wrapper_dependencies(infs, {}), [])

    def test_conditional_replay_damage_is_rejected_before_compilation(self) -> None:
        self.assertEqual(unbalanced_asl_conditionals("#ifndef FIXES\nlegacy\n#endif\n"), [])
        self.assertEqual(len(unbalanced_asl_conditionals("#ifndef FIXES\nlegacy\n")), 1)
        self.assertEqual(len(unbalanced_asl_conditionals("#endif\n")), 1)
        self.assertEqual(len(unbalanced_asl_conditionals("#if A\n#else\n#elif B\n#endif\n")), 1)
        self.assertEqual(unbalanced_asl_conditionals("/*\n#if unused\n*/\n#if A\n#if B\n#endif\n#else\n#endif\n"), [])

    def test_board_overlay_tracks_release_specific_asl_inputs(self) -> None:
        directory = "edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/AcpiPlatfomTables/"
        paths = {"src/" + directory + "40Pin-I2s.asl"}
        self.assertEqual(missing_board_table_inputs(paths), [])
        paths.add("custom/overlay/" + directory + "AcpiPlatfomTables.inf")
        self.assertEqual(len(missing_board_table_inputs(paths)), 1)
        paths.add("custom/overlay/" + directory + "40Pin-I2s.asl")
        self.assertEqual(missing_board_table_inputs(paths), [])

    def test_lto_link_path_requires_the_matching_support_archive(self) -> None:
        old = {"src/edk2/ArmPkg/Library/GccLto/liblto-aarch64.a"}
        new = {"src/edk2/BaseTools/Bin/GccLto/liblto-aarch64.a"}
        self.assertEqual(missing_lto_library(old, b"ArmPkg/Library/GccLto"), [])
        self.assertEqual(missing_lto_library(new, b"BaseTools/Bin/GccLto"), [])
        self.assertEqual(len(missing_lto_library(old, b"BaseTools/Bin/GccLto")), 1)
        self.assertEqual(len(missing_lto_library(new, b"ArmPkg/Library/GccLto")), 1)

    def test_configuration_manager_namespace_matches_its_headers(self) -> None:
        source = b"CM_ARCH_COMMON_CPC_INFO info; CIX_AML_PSD_INFO psd;"
        self.assertEqual(len(missing_configuration_manager_types(source, b"", b"")), 2)
        self.assertEqual(missing_configuration_manager_types(source, source, source), [])
        self.assertEqual(missing_configuration_manager_types(b"CM_ARM_CPC_INFO info; AML_PSD_INFO psd;", b"", b""), [])
        self.assertEqual(len(missing_configuration_manager_types(b"AML_PSD_INFO psd;", b"", b"CIX_AML_PSD_INFO")), 1)

    def test_smbios_cache_api_must_match_the_selected_edk2_header(self) -> None:
        modern = b"SMBIOS_CACHE_SIZE a; SMBIOS_CACHE_SIZE_2 b;"
        legacy = b"UINT16 a; UINT32 b;"
        header = b"typedef struct { UINT16 Size:15; } SMBIOS_CACHE_SIZE;\r\n"
        self.assertEqual(len(missing_smbios_cache_types(modern, b"")), 2)
        self.assertEqual(len(missing_smbios_cache_types(modern, header)), 1)
        header += b"typedef struct { UINT32 Size:31; } SMBIOS_CACHE_SIZE_2;\r\n"
        self.assertEqual(missing_smbios_cache_types(modern, header), [])
        self.assertEqual(missing_smbios_cache_types(legacy, b"UINT16 MaximumCacheSize;"), [])

    def test_inf_package_dependencies_follow_upstream_package_removal(self) -> None:
        infs = {"Driver.inf": "[Packages]\nSignedCapsulePkg/SignedCapsulePkg.dec\n[Sources]\nignored.dec\n"}
        self.assertEqual(missing_package_declarations({"src/edk2/SignedCapsulePkg/SignedCapsulePkg.dec"}, infs), [])
        self.assertEqual(len(missing_package_declarations(set(), infs)), 1)
        infs["Driver.inf"] = "[Packages]\nMdeModulePkg/MdeModulePkg.dec"
        self.assertEqual(missing_package_declarations({"src/edk2/MdeModulePkg/MdeModulePkg.dec"}, infs), [])

    def test_removed_toolchain_is_rejected_but_historical_toolchains_are_valid(self) -> None:
        old_make = b"build -a AARCH64 -t GCC5 -p board.dsc"
        # Old templates also define generic GCC flags; that does not create a
        # toolchain called GCC. Only an actual compiler-path rule does.
        old_tools = b"DEFINE GCC_AARCH64_CC_FLAGS = flags\r\n*_GCC5_AARCH64_CC_PATH = gcc\r\n"
        new_tools = b"DEFINE GCC_AARCH64_CC_FLAGS = flags\r\n*_GCC_AARCH64_CC_PATH = gcc\r\n"
        self.assertEqual(missing_toolchain(old_make, old_tools), [])
        self.assertEqual(len(missing_toolchain(old_make, new_tools)), 1)
        self.assertEqual(missing_toolchain(old_make.replace(b"GCC5", b"GCC"), new_tools), [])
        self.assertEqual(len(missing_toolchain(old_make.replace(b"GCC5", b"GCC"), old_tools)), 1)

    def test_platform_include_chain_rejects_removed_library(self) -> None:
        descriptors = {
            f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc": "!include Platform/CIX/Common.dsc.inc"
            for board in ("O6", "O6N")
        }
        descriptors["src/edk2-platforms/Platform/CIX/Common.dsc.inc"] = (
            "[LibraryClasses]\nCpuExceptionHandlerLib|ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf\n"
            "!if $(DISABLED_FEATURE) == TRUE\nOtherLib|Absent/Other.inf\n!endif\n"
        )
        paths = set(descriptors)
        problems = missing_platform_inputs(paths, descriptors, ())
        self.assertEqual(len(problems), 1)
        self.assertIn("ArmExceptionLib.inf", problems[0])
        paths.add("src/edk2/ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf")
        self.assertEqual(missing_platform_inputs(paths, descriptors, ()), [])

    def test_flash_layout_checks_release_specific_module_paths(self) -> None:
        descriptors = {f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc": ""
                       for board in ("O6", "O6N")}
        layout = "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6/O6.fdf"
        descriptors[layout] = "INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf\n"
        paths = set(descriptors) | {"src/edk2/ArmPkg/Drivers/ArmGic/ArmGicDxe.inf"}
        overlays = ("custom/overlay-experimental-uefi-settings",)
        problems = missing_platform_inputs(paths, descriptors, overlays)
        self.assertEqual(len(problems), 1)
        self.assertIn("ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf", problems[0])
        descriptors[layout] = "INF ArmPkg/Drivers/ArmGic/ArmGicDxe.inf\n"
        self.assertEqual(missing_platform_inputs(paths, descriptors, overlays), [])

    def test_platform_inputs_honor_overlay_precedence(self) -> None:
        base = {f"src/edk2-platforms/Platform/Radxa/Orion/{board}/{board}.dsc": "!include Platform/Common.dsc.inc" for board in ("O6", "O6N")}
        base["src/edk2-platforms/Platform/Common.dsc.inc"] = "Lib|Pkg/Removed.inf"
        overlay = "custom/overlay/edk2-platforms/Platform/Common.dsc.inc"
        base[overlay] = "Lib|Pkg/Present.inf"
        paths = set(base) | {"src/edk2/Pkg/Present.inf"}
        self.assertEqual(missing_platform_inputs(paths, base, ("custom/overlay",)), [])
        base[overlay] = "!include Platform/Missing.dsc.inc"
        self.assertEqual(missing_platform_inputs(paths, base, ("custom/overlay",)), ["missing platform input: Platform/Missing.dsc.inc"])

    def test_each_required_build_fix_is_checked_independently(self) -> None:
        contents = {}
        for _, path, marker, _ in BUILD_FIXES:
            contents[path] = contents.get(path, b"") + marker + b"\n"
        self.assertEqual(missing_build_fixes(contents), [])
        for commit, path, marker, _ in BUILD_FIXES:
            broken = dict(contents)
            broken[path] = broken[path].replace(marker, b"missing")
            self.assertTrue(any(commit in problem for problem in missing_build_fixes(broken)))
        contents.pop("scripts/ensure_iasl.sh")
        self.assertEqual(missing_build_fixes(contents), [])

    def test_one_inf_does_not_cover_another_module_in_the_same_directory(self) -> None:
        directory = "edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/"
        paths = {
            "src/" + directory + "FwVersionDxe.inf",
            "src/" + directory + "FwVersionProtocolTest.inf",
            "custom/overlay/" + directory + "FwVersionDxe.inf",
        }
        missing = "custom/overlay/" + directory + "FwVersionProtocolTest.inf"
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), [missing])
        paths.add(missing)
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), [])

    def test_unshadowed_modules_and_unrelated_directories_remain_valid(self) -> None:
        paths = {"src/edk2/Pkg/Driver/A.inf", "custom/overlay/edk2/Pkg/Other/file.c"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), [])

    def test_library_sibling_does_not_generate_ffs_inf_prerequisite(self) -> None:
        library = "src/edk2/Pkg/Library/Runtime.inf"
        paths = {library, "custom/overlay/edk2/Pkg/Library/Boot.inf"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay", {library}), [])

    def test_experimental_overlay_is_checked_independently(self) -> None:
        paths = {"src/edk2/Pkg/Driver/A.inf", "custom/overlay-experimental-uefi-settings/edk2/Pkg/Driver/file.c"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay-experimental-uefi-settings"), [
            "custom/overlay-experimental-uefi-settings/edk2/Pkg/Driver/A.inf",
        ])

    def test_nested_data_directory_also_shadows_module_root(self) -> None:
        paths = {"src/edk2/Pkg/Driver/A.inf", "custom/overlay/edk2/Pkg/Driver/Data/settings.bin"}
        self.assertEqual(missing_module_infs(paths, "custom/overlay"), ["custom/overlay/edk2/Pkg/Driver/A.inf"])

    def test_git_tree_audit_resolves_both_overlay_layers_and_rejects_broken_links(self) -> None:
        with tempfile.TemporaryDirectory(prefix="source-input-links.") as directory:
            repo = Path(directory)
            git(repo, "init", "-b", "build")
            git(repo, "config", "user.name", "Test")
            git(repo, "config", "user.email", "test")
            relative = "edk2/Pkg/Driver/A.inf"
            write_file(repo, "src/" + relative, "[Defines]\n  MODULE_TYPE = DXE_DRIVER\n")
            normal = repo / "custom/overlay" / relative
            experimental = repo / "custom/overlay-experimental-uefi-settings" / relative
            for path, target in ((normal, repo / "src" / relative), (experimental, normal)):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.symlink_to(os.path.relpath(target, path.parent))
            commit_all(repo, "valid chained overlays")
            self.assertFalse(any("symlink" in problem for problem in source_input_problems(repo, "HEAD")))
            experimental.unlink()
            experimental.symlink_to("missing.inf")
            commit_all(repo, "broken overlay")
            self.assertTrue(any("unresolvable overlay symlink" in problem for problem in source_input_problems(repo, "HEAD")))


if __name__ == "__main__":
    unittest.main()
