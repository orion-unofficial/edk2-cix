/** @file

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved
  Copyright (c) 2011-2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PrePi.h"

#include <Library/ArmGicLib.h>

#include <Ppi/ArmMpCoreInfo.h>

// Upstream retired the SEC ArmGicLib/ArmGicArchLib instances in 202502,
// but Sky1 PrePi still needs a small GIC bring-up subset before DXE.
STATIC
VOID
ArmGicEnableDistributor (
  IN UINTN  GicDistributorBase
  )
{
  if ((MmioRead32 (GicDistributorBase + ARM_GIC_ICDDCR) & ARM_GIC_ICDDCR_ARE) != 0) {
    MmioOr32 (GicDistributorBase + ARM_GIC_ICDDCR, 0x2);
  } else {
    MmioOr32 (GicDistributorBase + ARM_GIC_ICDDCR, 0x1);
  }
}

STATIC
VOID
ArmGicSendSgiTo (
  IN UINTN  GicDistributorBase,
  IN UINTN  TargetListFilter,
  IN UINTN  CPUTargetList,
  IN UINTN  SgiId
  )
{
  MmioWrite32 (
    GicDistributorBase + ARM_GIC_ICDSGIR,
    ((TargetListFilter & 0x3) << 24) | ((CPUTargetList & 0xFF) << 16) | (SgiId & 0xF)
    );
}

STATIC
UINTN
ArmGicAcknowledgeInterrupt (
  IN  UINTN  GicInterruptInterfaceBase,
  OUT UINTN  *InterruptId
  )
{
  UINTN  Value;

#if defined (__aarch64__) || defined (MDE_CPU_AARCH64)
  (VOID)GicInterruptInterfaceBase;
  __asm__ volatile ("mrs %0, ICC_IAR1_EL1" : "=r" (Value));
  if (InterruptId != NULL) {
    *InterruptId = Value;
  }
#else
  Value = MmioRead32 (GicInterruptInterfaceBase + ARM_GIC_ICCIAR);
  if (InterruptId != NULL) {
    *InterruptId = Value & ARM_GIC_ICCIAR_ACKINTID;
  }
#endif

  return Value;
}

STATIC
VOID
ArmGicEndOfInterrupt (
  IN UINTN  GicInterruptInterfaceBase,
  IN UINTN  Source
  )
{
#if defined (__aarch64__) || defined (MDE_CPU_AARCH64)
  (VOID)GicInterruptInterfaceBase;
  __asm__ volatile ("msr ICC_EOIR1_EL1, %0" :: "r" (Source));
  __asm__ volatile ("isb");
#else
  MmioWrite32 (GicInterruptInterfaceBase + ARM_GIC_ICCEIOR, Source);
#endif
}

STATIC
UINTN
ArmGicGetMaxNumInterrupts (
  IN UINTN  GicDistributorBase
  )
{
  UINTN  ItLines;

  ItLines = MmioRead32 (GicDistributorBase + ARM_GIC_ICDICTR) & 0x1F;
  return (ItLines == 0x1f) ? 1020 : 32 * (ItLines + 1);
}

VOID
PrimaryMain (
  IN  UINTN   UefiMemoryBase,
  IN  UINTN   StacksBase,
  IN  UINT64  StartTimeStamp
  )
{
  // Enable the GIC Distributor
  ArmGicEnableDistributor (PcdGet64 (PcdGicDistributorBase));

  // In some cases, the secondary cores are waiting for an SGI from the next stage boot loader to resume their initialization
  if (!FixedPcdGet32 (PcdSendSgiToBringUpSecondaryCores)) {
    // Sending SGI to all the Secondary CPU interfaces
    ArmGicSendSgiTo (PcdGet64 (PcdGicDistributorBase), ARM_GIC_ICDSGIR_FILTER_EVERYONEELSE, 0x0E, PcdGet32 (PcdGicSgiIntId));
  }

  PrePiMain (UefiMemoryBase, StacksBase, StartTimeStamp);

  // We must never return
  ASSERT (FALSE);
}

VOID
SecondaryMain (
  IN  UINTN  MpId
  )
{
  EFI_STATUS            Status;
  ARM_MP_CORE_INFO_PPI  *ArmMpCoreInfoPpi;
  UINTN                 Index;
  UINTN                 ArmCoreCount;
  ARM_CORE_INFO         *ArmCoreInfoTable;
  UINT32                ClusterId;
  UINT32                CoreId;

  VOID  (*SecondaryStart)(
    VOID
    );
  UINTN  SecondaryEntryAddr;
  UINTN  AcknowledgeInterrupt;
  UINTN  InterruptId;

  ClusterId = GET_CLUSTER_ID (MpId);
  CoreId    = GET_CORE_ID (MpId);

  // On MP Core Platform we must implement the ARM MP Core Info PPI (gArmMpCoreInfoPpiGuid)
  Status = GetPlatformPpi (&gArmMpCoreInfoPpiGuid, (VOID **)&ArmMpCoreInfoPpi);
  ASSERT_EFI_ERROR (Status);

  ArmCoreCount = 0;
  Status       = ArmMpCoreInfoPpi->GetMpCoreInfo (&ArmCoreCount, &ArmCoreInfoTable);
  ASSERT_EFI_ERROR (Status);

  // Find the core in the ArmCoreTable
  for (Index = 0; Index < ArmCoreCount; Index++) {
    if ((GET_MPIDR_AFF1 (ArmCoreInfoTable[Index].Mpidr) == ClusterId) &&
        (GET_MPIDR_AFF0 (ArmCoreInfoTable[Index].Mpidr) == CoreId))
    {
      break;
    }
  }

  // The ARM Core Info Table must define every core
  ASSERT (Index != ArmCoreCount);

  // Clear Secondary cores MailBox
  MmioWrite32 (ArmCoreInfoTable[Index].MailboxClearAddress, ArmCoreInfoTable[Index].MailboxClearValue);

  do {
    ArmCallWFI ();

    // Read the Mailbox
    SecondaryEntryAddr = MmioRead32 (ArmCoreInfoTable[Index].MailboxGetAddress);

    // Acknowledge the interrupt and send End of Interrupt signal.
    AcknowledgeInterrupt = ArmGicAcknowledgeInterrupt (PcdGet64 (PcdGicInterruptInterfaceBase), &InterruptId);
    // Check if it is a valid interrupt ID
    if (InterruptId < ArmGicGetMaxNumInterrupts (PcdGet64 (PcdGicDistributorBase))) {
      // Got a valid SGI number hence signal End of Interrupt
      ArmGicEndOfInterrupt (PcdGet64 (PcdGicInterruptInterfaceBase), AcknowledgeInterrupt);
    }
  } while (SecondaryEntryAddr == 0);

  // Jump to secondary core entry point.
  SecondaryStart = (VOID (*)()) SecondaryEntryAddr;
  SecondaryStart ();

  // The secondaries shouldn't reach here
  ASSERT (FALSE);
}
