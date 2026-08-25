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
            for path in root.rglob("*.asl"):
                content = path.read_text(encoding="utf-8", errors="replace")
                if printf_call.search(content):
                    failures.append(path.relative_to(REPO_ROOT).as_posix())

        self.assertEqual(
            failures,
            [],
            "ACPICA 20260408 no longer supports the printf/fprintf ASL extension",
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
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionDxe.inf", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionProtocolTest.c", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/FwVersionProtocolTest.inf", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/SmbiosType45.c", True),
            ("custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/FwVersionDxe/UefiMemRecords.h", True),
        )
        for relative_path, expect_symlink in cases:
            with self.subTest(path=relative_path):
                self.assertEqual((REPO_ROOT / relative_path).is_symlink(), expect_symlink)


if __name__ == "__main__":
    unittest.main()
