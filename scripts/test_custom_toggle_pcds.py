#!/usr/bin/env python3

import pathlib
import re
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


def read_repo_text(relative_path: str) -> str:
    return (REPO_ROOT / relative_path).read_text(encoding="utf-8")


class CustomTogglePcdsTest(unittest.TestCase):
    def test_cix_asl_avoids_retired_printf_compiler_extension(self) -> None:
        roots = (
            REPO_ROOT / "src/edk2-platforms/Platform/CIX",
            REPO_ROOT / "custom/overlay/edk2-platforms/Platform/CIX",
            REPO_ROOT
            / "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/CIX",
        )
        printf_call = re.compile(r"\bf?printf\s*\(", re.IGNORECASE)
        failures: list[str] = []

        for root in roots:
            for pattern in ("*.asl", "*.aslc"):
                for path in root.rglob(pattern):
                    for line_number, line in enumerate(
                        path.read_text(encoding="utf-8").splitlines(),
                        start=1,
                    ):
                        if printf_call.search(line):
                            failures.append(
                                f"{path.relative_to(REPO_ROOT)}:{line_number}: {line.strip()}"
                            )

        self.assertEqual(
            failures,
            [],
            "ACPICA 20260408 no longer emits AML for Printf/Fprintf:\n"
            + "\n".join(failures),
        )

    def test_dbg2_uses_uart3_pcd(self) -> None:
        content = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Dbg2.aslc"
        )
        self.assertIn("FixedPcdGetBool (PcdAcpiUart3Enable)", content)
        self.assertNotIn("#if DEBUG_ON_UART3", content)

    def test_custom_sky1_common_files_do_not_inject_uart_toggle_cc_flags(self) -> None:
        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc",
            "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertNotIn("GCC:*_*_*_CC_FLAGS          = -DUART3_ENABLE=1", content)
                self.assertNotIn("GCC:*_*_*_CC_FLAGS          = -DDEBUG_ON_UART3=1", content)
                self.assertNotIn("GCC:*_*_*_ASLPP_FLAGS       = -DDEBUG_ON_UART3=1", content)
                self.assertNotIn("GCC:*_*_*_CC_FLAGS          = -UUART3_ENABLE", content)
                self.assertNotIn("GCC:*_*_*_CC_FLAGS          = -UDEBUG_ON_UART3", content)
                self.assertNotIn("GCC:*_*_*_ASLPP_FLAGS       = -UDEBUG_ON_UART3", content)
                self.assertIn("gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable|TRUE", content)
                self.assertIn("gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable|FALSE", content)
                self.assertNotIn("GCC:*_*_*_CC_FLAGS          = -DENABLE_FIRMWARE_FIXES=1", content)
                self.assertNotIn("GCC:*_*_*_CC_FLAGS          = -UENABLE_FIRMWARE_FIXES", content)

    def test_custom_platform_env_hooks_use_uart3_pcd(self) -> None:
        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.c",
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c",
            "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("FixedPcdGetBool (PcdAcpiUart3Enable)", content)
                self.assertNotIn("#if UART3_ENABLE", content)

        experimental_o6 = read_repo_text(
            "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLibCustom.c"
        )
        self.assertIn(
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.c",
            experimental_o6,
        )

    def test_custom_platform_env_hook_infs_declare_uart3_pcd(self) -> None:
        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf",
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf",
            "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf",
            "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/PlatformEnvHookLib.inf",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("gCixTokenSpaceGuid.PcdAcpiUart3Enable", content)

    def test_custom_firmware_fixes_policy_uses_pcd(self) -> None:
        acpi_platform = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/Radxa/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.c"
        )
        self.assertIn("FixedPcdGetBool (PcdCustomFirmwareFixesEnable)", acpi_platform)
        self.assertNotIn("#ifdef ENABLE_FIRMWARE_FIXES", acpi_platform)

        acpi_platform_inf = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/Radxa/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.inf"
        )
        self.assertIn("gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable", acpi_platform_inf)

        acpi_tables_inf = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiSocTables.inf"
        )
        self.assertIn("gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable", acpi_tables_inf)

        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Dsdt-Pcie.asl",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Dsdt-CdnsPcie.asl",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Dsdt-CPU.asl",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Iort.aslc",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("PcdCustomFirmwareFixesEnable", content)

        dsdt = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Dsdt.asl"
        )
        self.assertIn("#ifdef ENABLE_FIRMWARE_FIXES", dsdt)
        self.assertIn('include("Dsdt-BusPerf.asl")', dsdt)

    def test_custom_firmware_fix_overlays_move_remaining_policy_churn_out_of_shared_sources(self) -> None:
        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/SmbiosMiscLib/SmbiosMiscLib.c",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/UpdateDsdt.c",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/UpdateDsdt.c",
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/PlatformSmbios.c",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("PcdCustomFirmwareFixesEnable", content)
                self.assertNotIn("ENABLE_FIRMWARE_FIXES", content)

        platform_smbios_header = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/PlatformSmbios.h"
        )
        self.assertNotIn("ENABLE_FIRMWARE_FIXES", platform_smbios_header)

    def test_custom_cppc_reference_performance_is_runtime_repaired(self) -> None:
        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/UpdateDsdt.c",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/UpdateDsdt.c",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("CPC_REFERENCE_PERFORMANCE_OFFSET", content)
                self.assertIn("ArmGenericTimerGetTimerFreq", content)
                self.assertIn("UseFirmwareFixes && (ReferencePerf != 0)", content)

        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/AcpiSocDxe.inf",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.inf",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("ArmGenericTimerCounterLib", content)

    def test_pcie_smmu_is_enabled_only_with_firmware_fixes(self) -> None:
        common_defines = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/CommonDefines.h"
        )
        self.assertIn("#ifdef ENABLE_FIRMWARE_FIXES", common_defines)
        self.assertIn("#define PCIE_SMMU_ENABLE        1", common_defines)
        self.assertIn("#define PCIE_SMMU_ENABLE        0", common_defines)

        makefile = read_repo_text("src/Makefile")
        self.assertIn(
            "DEBUG_GCC_AARCH64_ASLCC_FLAGS   = "
            "DEF(GCC_ASLCC_FLAGS) -DENABLE_FIRMWARE_FIXES=1",
            makefile,
        )
        self.assertIn(
            "RELEASE_GCC_AARCH64_ASLCC_FLAGS   = "
            "DEF(GCC_ASLCC_FLAGS) -DENABLE_FIRMWARE_FIXES=1",
            makefile,
        )

    def test_cpu_performance_order_and_static_cppc_classes_match_sky1(self) -> None:
        configuration_manager = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/"
            "ConfigurationManagerDxe/ConfigurationManager.c"
        )
        self.assertIn(
            "8,  9, 10, 11, 4, 5, 6, 7, 2, 3, 0, 1",
            configuration_manager,
        )

        dsdt = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/"
            "AcpiSocTables/Dsdt-CPU.asl"
        )
        cppc_lines = [
            line.strip()
            for line in dsdt.splitlines()
            if "CPPC_PACKAGE_INIT" in line and "CIX_A" in line
        ]
        self.assertEqual(len(cppc_lines), 12)
        self.assertTrue(all("CIX_A520_REF_PERF" in line for line in cppc_lines[:4]))
        self.assertTrue(all("CIX_A720_REF_PERF" in line for line in cppc_lines[4:]))

    def test_custom_version_reporting_is_overlay_only(self) -> None:
        custom_fw_version = read_repo_text(
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionDxe.c"
        )
        self.assertIn("PcdFirmwareVersionString", custom_fw_version)
        self.assertIn("InitUefiVersionHeader", custom_fw_version)

        shared_fw_version = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionDxe.c"
        )
        self.assertIn("STR (UEFI_FW_VERSION)", shared_fw_version)
        self.assertNotIn("InitUefiVersionHeader", shared_fw_version)

        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/SmbiosType0.c",
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/PlatformSmbios/SmbiosType0.c",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("PcdFirmwareVersionString", content)

        for relative_path in (
            "src/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/SmbiosType0.c",
            "src/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/PlatformSmbios/SmbiosType0.c",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("STR (UEFI_FW_VERSION)", content)
                self.assertNotIn("PcdFirmwareVersionString", content)

    def test_custom_firmware_fix_overlay_infs_declare_the_fixed_pcd_dependency(self) -> None:
        for relative_path in (
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/SmbiosMiscLib/SmbiosMiscLib.inf",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/AcpiSocDxe.inf",
            "custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.inf",
            "custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/PlatformSmbios.inf",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn("gCixTokenSpaceGuid.PcdCustomFirmwareFixesEnable", content)

    def test_custom_policy_overlay_materialization_keeps_unchanged_companions_as_symlinks(self) -> None:
        cases = (
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/SmbiosMiscLib/SmbiosMiscLib.c", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Library/SmbiosMiscLib/SmbiosMiscLib.inf", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/AcpiSocDxe.c", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/AcpiSocDxe.h", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/AcpiSocDxe.inf", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocDxe/UpdateDsdt.c", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.c", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.h", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.inf", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiPlatformDxe/UpdateDsdt.c", False),
            ("custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/PlatformSmbios.c", False),
            ("custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/PlatformSmbios.h", False),
            ("custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/PlatformSmbios/SmbiosType0.c", False),
            ("custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6N/Drivers/PlatformSmbios/SmbiosType0.c", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionDxe.c", False),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/SmbiosType45.c", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/UefiMemRecords.h", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiMpamTables.inf", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Mpam.aslc", True),
        )
        for relative_path, expect_symlink in cases:
            with self.subTest(path=relative_path):
                self.assertEqual((REPO_ROOT / relative_path).is_symlink(), expect_symlink)

    def test_mpam_is_gated_and_links_to_the_shared_l3(self) -> None:
        dsc = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc"
        )
        fdf = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Sky1Common.fdf.inc"
        )
        self.assertIn("!if $(ENABLE_FIRMWARE_FIXES) == TRUE", dsc)
        self.assertIn(
            "Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiMpamTables.inf",
            dsc,
        )
        self.assertIn("!if $(ENABLE_FIRMWARE_FIXES) == TRUE", fdf)
        self.assertIn(
            "INF RuleOverride = ACPITABLE Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiMpamTables.inf",
            fdf,
        )
        for relative_path in (
            "src/edk2-platforms/Platform/Radxa/Orion/O6/O6.fdf",
            "src/edk2-platforms/Platform/Radxa/Orion/O6N/O6N.fdf",
            "src/edk2-platforms/Platform/CIX/Sky1/Merak/Merak.fdf",
            "src/edk2-platforms/Platform/CIX/Sky1/Edge/Edge.fdf",
        ):
            with self.subTest(path=relative_path):
                board_fdf = read_repo_text(relative_path)
                self.assertIn("RAW ASL Optional       |.aml", board_fdf)

        mpam = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/Mpam.aslc"
        )
        mpam_inf = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/AcpiMpamTables.inf"
        )
        self.assertIn("Silicon/CIX/Sky1/CixPkg.dec", mpam_inf)
        self.assertIn("#define SKY1_MPAM_MSC_BASE        0x0F010000ULL", mpam)
        self.assertIn("#define SKY1_MPAM_MSC_SIZE        0x00010000U", mpam)
        self.assertIn("#define SKY1_MPAM_SHARED_L3_ID    1", mpam)
        self.assertIn("EFI_ACPI_MPAM_LOCATION_PROCESSOR_CACHE", mpam)

        pptt = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Library/Acpi/CIX/AcpiPpttLibCIX/PpttGenerator.c"
        )
        self.assertIn("#define CIX_PPTT_SHARED_L3_CACHE_ID       1U", pptt)
        self.assertIn(
            "CacheNode->Flags.CacheIdValid = EFI_ACPI_6_4_PPTT_CACHE_ID_VALID;",
            pptt,
        )
        self.assertIn("CacheNode->CacheId            = CIX_PPTT_SHARED_L3_CACHE_ID;", pptt)
        self.assertEqual(
            pptt.count(
                "CacheNode->Flags.CacheIdValid = EFI_ACPI_6_4_PPTT_CACHE_ID_VALID;"
            ),
            1,
        )
        self.assertEqual(
            pptt.count(
                "CacheNode->CacheId            = CIX_PPTT_SHARED_L3_CACHE_ID;"
            ),
            1,
        )

    def test_o6_hda_gpio_output_is_firmware_fix_only(self) -> None:
        o6 = read_repo_text("src/edk2-platforms/Platform/Radxa/Orion/O6/O6.dsc")
        self.assertIn("!if $(ENABLE_FIRMWARE_FIXES) == TRUE", o6)
        self.assertIn("PcdAcpiGpio3IoMask|0x00018020", o6)
        self.assertIn("PcdAcpiGpio3IoMask|0x00018000", o6)

        o6n = read_repo_text("src/edk2-platforms/Platform/Radxa/Orion/O6N/O6N.dsc")
        self.assertNotIn("PcdAcpiGpio3IoMask|0x00018020", o6n)
        self.assertIn("PcdAcpiGpio3IoMask|0x00018000", o6n)

    def test_sky1_arm_lib_uses_the_current_edk2_provider(self) -> None:
        common = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc"
        )
        provider = "MdePkg/Library/ArmLib/ArmBaseLib.inf"
        self.assertIn(f"ArmLib|{provider}", common)
        self.assertNotIn("ArmPkg/Library/ArmLib/ArmBaseLib.inf", common)
        self.assertTrue((REPO_ROOT / "src" / "edk2" / provider).is_file())

    def test_radxa_capsule_library_uses_the_retained_sky1_provider(self) -> None:
        provider = (
            "Platform/CIX/Sky1/Library/EdkiiSystemCapsuleLib/"
            "EdkiiSystemCapsuleLib.inf"
        )
        for relative_path in (
            "src/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc",
            "custom/overlay/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc",
            "custom/overlay-experimental-uefi-settings/edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Sky1Common.dsc.inc",
        ):
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                self.assertIn(f"EdkiiSystemCapsuleLib|{provider}", content)
                self.assertNotIn(
                    "SignedCapsulePkg/Library/EdkiiSystemCapsuleLib",
                    content,
                )
        self.assertTrue((REPO_ROOT / "src" / "edk2-platforms" / provider).is_file())

    def test_signed_capsule_compatibility_is_platform_scoped(self) -> None:
        dec = read_repo_text(
            "src/edk2-platforms/Platform/CIX/Sky1/Sky1.dec"
        )
        for declaration in (
            "EdkiiSystemCapsuleLib|Include/Library/EdkiiSystemCapsuleLib.h",
            "gEfiSignedCapsulePkgTokenSpaceGuid =",
            "PcdEdkiiPkcs7TestPublicKeyFileGuid|",
            "PcdEdkiiSystemFirmwareImageDescriptor|",
            "PcdEdkiiSystemFirmwareFileGuid|",
        ):
            self.assertIn(declaration, dec)

        for relative_path in (
            "src/edk2-platforms/Platform/CIX/Sky1/Include/Guid/EdkiiSystemFmpCapsule.h",
            "src/edk2-platforms/Platform/CIX/Sky1/Include/Library/EdkiiSystemCapsuleLib.h",
            "src/edk2-platforms/Platform/CIX/Sky1/Include/Library/PlatformFlashAccessLib.h",
        ):
            self.assertTrue((REPO_ROOT / relative_path).is_file())

        live_files = (
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/SystemFirmwareUpdate/SystemFirmwareReportDxe.inf",
            "src/edk2-platforms/Platform/CIX/Sky1/Drivers/ABLUpdate/SystemFirmwareReportDxe.inf",
            "src/edk2-platforms/Platform/CIX/Sky1/Library/EdkiiSystemCapsuleLib/EdkiiSystemCapsuleLib.inf",
            "src/edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc",
        )
        for relative_path in live_files:
            with self.subTest(path=relative_path):
                content = read_repo_text(relative_path)
                live_lines = (
                    line for line in content.splitlines()
                    if not line.lstrip().startswith("#")
                )
                self.assertNotIn(
                    "SignedCapsulePkg/",
                    "\n".join(live_lines),
                )


if __name__ == "__main__":
    unittest.main()
