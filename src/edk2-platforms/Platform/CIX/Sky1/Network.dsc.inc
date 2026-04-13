## @file
# Network DSC include for CIX/Radxa Sky1 platforms.
#
# These platforms carry setup code that consumes the PXE/HTTP support PCDs as
# FixedAtBuild values, so keep the upstream network include on the fixed-only
# path after NetworkPkg split the PXE defaults into a dynamic include.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
!include NetworkPkg/NetworkDefines.dsc.inc

!ifndef PLATFORMX64_ENABLE
  DEFINE PLATFORMX64_ENABLE = FALSE
!endif

[PcdsFixedAtBuild]
!include NetworkPkg/NetworkFixedPcds.dsc.inc

[LibraryClasses]
!include NetworkPkg/NetworkLibs.dsc.inc

[BuildOptions]
!include NetworkPkg/NetworkBuildOptions.dsc.inc

!if $(PLATFORMX64_ENABLE) == TRUE
[Components.X64]
!include NetworkPkg/NetworkComponents.dsc.inc
!else
[Components.IA32, Components.X64, Components.ARM, Components.AARCH64, Components.RISCV64, Components.LOONGARCH64]
!include NetworkPkg/NetworkComponents.dsc.inc
!endif
