/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.
  Copyright (c) 2011-2017, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/TimerLib.h>
#include <Library/PerformanceLib.h>
#include <Library/CixPostCodeLib.h>
#include <Library/CixFwBootPerfLib.h>
#include <Ppi/GuidedSectionExtraction.h>
#include <Ppi/ArmMpCoreInfo.h>
#include <Ppi/SecPerformance.h>

#include "PrePi.h"

#define STR(x)   XSTR (x)
#define XSTR(x)  #x
#define BUILD_TIME_TEMPLATE  "HH:MM:SS"
#define BUILD_DATE_TEMPLATE  "Mon DD YYYY"

#define IS_XIP()  (((UINT64)FixedPcdGet64 (PcdFdBaseAddress) > mSystemMemoryEnd) ||\
                  ((FixedPcdGet64 (PcdFdBaseAddress) + FixedPcdGet32 (PcdFdSize)) <= FixedPcdGet64 (PcdSystemMemoryBase)))

UINT64  mSystemMemoryEnd = FixedPcdGet64 (PcdSystemMemoryBase) +
                           FixedPcdGet64 (PcdSystemMemorySize) - 1;

STATIC
BOOLEAN
ResolveCompileBuildTimestamp (
  OUT CHAR8  *BuildTime,
  IN  UINTN  BuildTimeSize,
  OUT CHAR8  *BuildDate,
  IN  UINTN  BuildDateSize
  )
{
#ifdef COMPILE_BUILD_DATE
  STATIC CONST CHAR8  IsoBuildDate[] = STR (COMPILE_BUILD_DATE);
  STATIC CONST CHAR8  *MonthNames[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  UINTN               MonthIndex;
  UINTN               MonthValue;

  if ((BuildTimeSize < sizeof (BUILD_TIME_TEMPLATE)) ||
      (BuildDateSize < sizeof (BUILD_DATE_TEMPLATE)) ||
      (AsciiStrLen (IsoBuildDate) < 19) ||
      (IsoBuildDate[4] != '-') ||
      (IsoBuildDate[7] != '-') ||
      (IsoBuildDate[10] != 'T') ||
      (IsoBuildDate[13] != ':') ||
      (IsoBuildDate[16] != ':'))
  {
    return FALSE;
  }

  CopyMem (BuildTime, &IsoBuildDate[11], 8);
  BuildTime[8] = '\0';

  if ((IsoBuildDate[5] < '0') || (IsoBuildDate[5] > '1') ||
      (IsoBuildDate[6] < '0') || (IsoBuildDate[6] > '9'))
  {
    return FALSE;
  }

  MonthValue = ((UINTN)(IsoBuildDate[5] - '0') * 10U) + (UINTN)(IsoBuildDate[6] - '0');
  if ((MonthValue == 0) || (MonthValue > 12)) {
    return FALSE;
  }

  MonthIndex = MonthValue - 1;
  CopyMem (BuildDate, MonthNames[MonthIndex], 3);
  BuildDate[3] = ' ';
  BuildDate[4] = IsoBuildDate[8];
  BuildDate[5] = IsoBuildDate[9];
  BuildDate[6] = ' ';
  CopyMem (&BuildDate[7], &IsoBuildDate[0], 4);
  BuildDate[11] = '\0';
  return TRUE;
#else
  return FALSE;
#endif
}

EFI_STATUS
GetPlatformPpi (
  IN  EFI_GUID  *PpiGuid,
  OUT VOID      **Ppi
  )
{
  UINTN                   PpiListSize;
  UINTN                   PpiListCount;
  EFI_PEI_PPI_DESCRIPTOR  *PpiList;
  UINTN                   Index;

  PpiListSize = 0;
  ArmPlatformGetPlatformPpiList (&PpiListSize, &PpiList);
  PpiListCount = PpiListSize / sizeof (EFI_PEI_PPI_DESCRIPTOR);
  for (Index = 0; Index < PpiListCount; Index++, PpiList++) {
    if (CompareGuid (PpiList->Guid, PpiGuid) == TRUE) {
      *Ppi = PpiList->Ppi;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

VOID
PrePiMain (
  IN  UINTN   UefiMemoryBase,
  IN  UINTN   StacksBase,
  IN  UINT64  StartTimeStamp
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE  *HobList;
  ARM_MP_CORE_INFO_PPI        *ArmMpCoreInfoPpi;
  UINTN                       ArmCoreCount;
  ARM_CORE_INFO               *ArmCoreInfoTable;
  EFI_STATUS                  Status;
  CHAR8                       Buffer[100];
  CHAR8                       BuildDate[sizeof (BUILD_DATE_TEMPLATE)];
  CHAR8                       BuildTime[sizeof (BUILD_TIME_TEMPLATE)];
  UINTN                       CharCount;
  UINTN                       StacksSize;
  CONST CHAR8                 *DisplayBuildDate;
  CONST CHAR8                 *DisplayBuildTime;
  FIRMWARE_SEC_PERFORMANCE    Performance;

  // If ensure the FD is either part of the System Memory or totally outside of the System Memory (XIP)
  ASSERT (
    IS_XIP () ||
    ((FixedPcdGet64 (PcdFdBaseAddress) >= FixedPcdGet64 (PcdSystemMemoryBase)) &&
     ((UINT64)(FixedPcdGet64 (PcdFdBaseAddress) + FixedPcdGet32 (PcdFdSize)) <= (UINT64)mSystemMemoryEnd))
    );

  // Initialize the architecture specific bits
  ArchInitialize ();

  // Initialize the Serial Port
  SerialPortInitialize ();
  POST_CODE(PrePiStart);
  DisplayBuildTime = __TIME__;
  DisplayBuildDate = __DATE__;
  if (ResolveCompileBuildTimestamp (BuildTime, sizeof (BuildTime), BuildDate, sizeof (BuildDate))) {
    DisplayBuildTime = BuildTime;
    DisplayBuildDate = BuildDate;
  }

  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "Cix UEFI firmware (version %s built at %a on %a)\n\r",
                (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString),
                DisplayBuildTime,
                DisplayBuildDate
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);

  // Initialize the Debug Agent for Source Level Debugging
  InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, NULL, NULL);
  SaveAndSetDebugTimerInterrupt (TRUE);

  // Declare the PI/UEFI memory region
  HobList = HobConstructor (
              (VOID *)UefiMemoryBase,
              FixedPcdGet32 (PcdSystemMemoryUefiRegionSize),
              (VOID *)UefiMemoryBase,
              (VOID *)StacksBase // The top of the UEFI Memory is reserved for the stacks
              );
  PrePeiSetHobList (HobList);

  // Initialize MMU and Memory HOBs (Resource Descriptor HOBs)
  Status = MemoryPeim (UefiMemoryBase, FixedPcdGet32 (PcdSystemMemoryUefiRegionSize));
  ASSERT_EFI_ERROR (Status);

  // Create the Stacks HOB (reserve the memory for all stacks)
  if (ArmIsMpCore ()) {
    StacksSize = PcdGet32 (PcdCPUCorePrimaryStackSize) +
                 ((FixedPcdGet32 (PcdCoreCount) - 1) * FixedPcdGet32 (PcdCPUCoreSecondaryStackSize));
  } else {
    StacksSize = PcdGet32 (PcdCPUCorePrimaryStackSize);
  }

  BuildStackHob (StacksBase, StacksSize);

  // check later: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));

  if (ArmIsMpCore ()) {
    // Only MP Core platform need to produce gArmMpCoreInfoPpiGuid
    Status = GetPlatformPpi (&gArmMpCoreInfoPpiGuid, (VOID **)&ArmMpCoreInfoPpi);

    // On MP Core Platform we must implement the ARM MP Core Info PPI (gArmMpCoreInfoPpiGuid)
    ASSERT_EFI_ERROR (Status);

    // Build the MP Core Info Table
    ArmCoreCount = 0;
    Status       = ArmMpCoreInfoPpi->GetMpCoreInfo (&ArmCoreCount, &ArmCoreInfoTable);
    if (!EFI_ERROR (Status) && (ArmCoreCount > 0)) {
      // Build MPCore Info HOB
      BuildGuidDataHob (&gArmMpCoreInfoGuid, ArmCoreInfoTable, sizeof (ARM_CORE_INFO) * ArmCoreCount);
    }
  }

  // Store timer value logged at the beginning of firmware image execution
  Performance.ResetEnd = GetTimeInNanoSecond (StartTimeStamp);

  // Build SEC Performance Data Hob
  BuildGuidDataHob (&gEfiFirmwarePerformanceGuid, &Performance, sizeof (Performance));

  // Set the Boot Mode
  SetBootMode (ArmPlatformGetBootMode ());

  // Initialize Platform HOBs (CpuHob and FvHob)
  Status = PlatformPeim ();
  ASSERT_EFI_ERROR (Status);

  // Now, the HOB List has been initialized, we can register performance information
  PERF_START (NULL, "PEI", NULL, StartTimeStamp);

  // SEC phase needs to run library constructors by hand.
  ProcessLibraryConstructorList ();

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  Status = DecompressFirstFv ();
  ASSERT_EFI_ERROR (Status);
  POST_CODE(PrePiEnd);
  // Load the DXE Core and transfer control to it
  Status = LoadDxeCoreFromFv (NULL, 0);
  ASSERT_EFI_ERROR (Status);
}

VOID
CEntryPoint (
  IN  UINTN  MpId,
  IN  UINTN  UefiMemoryBase,
  IN  UINTN  StacksBase
  )
{
  UINT64  StartTimeStamp;

  cix_set_boot_phase (BLOADER_PHASE, RECORD_START);
  // Initialize the platform specific controllers
  ArmPlatformInitialize (MpId);

  if (ArmPlatformIsPrimaryCore (MpId) && PerformanceMeasurementEnabled ()) {
    // Initialize the Timer Library to setup the Timer HW controller
    TimerConstructor ();
    // We cannot call yet the PerformanceLib because the HOB List has not been initialized
    StartTimeStamp = GetPerformanceCounter ();
  } else {
    StartTimeStamp = 0;
  }

  // Data Cache enabled on Primary core when MMU is enabled.
  ArmDisableDataCache ();
  // Invalidate instruction cache
  ArmInvalidateInstructionCache ();
  // Enable Instruction Caches on all cores.
  ArmEnableInstructionCache ();

  // Define the Global Variable region when we are not running in XIP
  if (!IS_XIP ()) {
    if (ArmPlatformIsPrimaryCore (MpId)) {
      if (ArmIsMpCore ()) {
        // Signal the Global Variable Region is defined (event: ARM_CPU_EVENT_DEFAULT)
        ArmCallSEV ();
      }
    } else {
      // Wait the Primary core has defined the address of the Global Variable region (event: ARM_CPU_EVENT_DEFAULT)
      ArmCallWFE ();
    }
  }

  // If not primary Jump to Secondary Main
  if (ArmPlatformIsPrimaryCore (MpId)) {
    InvalidateDataCacheRange (
      (VOID *)UefiMemoryBase,
      FixedPcdGet32 (PcdSystemMemoryUefiRegionSize)
      );

    // Goto primary Main.
    PrimaryMain (UefiMemoryBase, StacksBase, StartTimeStamp);
  } else {
    SecondaryMain (MpId);
  }

  // DXE Core should always load and never return
  ASSERT (FALSE);
}
