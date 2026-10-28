/** @file
  Validate the CIX legacy secure-partition records before native EDK2 consumes them.

  CIX OP-TEE load_stmm() appends one CPU record and a flash-storage record to
  EFI_SECURE_PARTITION_BOOT_INFO. The NOR driver consumes the first 36 bytes
  of that final record in a GUID HOB; the native structure has tail padding.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef CIX_LEGACY_BOOT_INFO_H_
#define CIX_LEGACY_BOOT_INFO_H_

typedef struct {
  UINT64    Base[3];
  UINT32    Size[3];
} CIX_LEGACY_FLASH_INFO;

STATIC inline
BOOLEAN
CixLegacyRegion (
  UINT64  Base,
  UINT64  Size,
  UINT64  End
  )
{
  return (Size != 0) && ((Base & 4095) == 0) && ((Size & 4095) == 0) &&
         (Base <= End) && (Size == End - Base);
}

STATIC inline
CONST CIX_LEGACY_FLASH_INFO *
CixLegacyFlashInfo (
  CONST EFI_SECURE_PARTITION_BOOT_INFO  *Info
  )
{
  return (CONST CIX_LEGACY_FLASH_INFO *)(Info->CpuInfo + 1);
}

STATIC inline
BOOLEAN
CixValidateLegacyBootInfo (
  CONST VOID  *Address,
  UINT64      Length
  )
{
  CONST EFI_SECURE_PARTITION_BOOT_INFO  *Info;
  CONST CIX_LEGACY_FLASH_INFO           *Flash;
  UINTN                               Required;
  UINTN                               Index;

  Required = sizeof (*Info) + sizeof (EFI_SECURE_PARTITION_CPU_INFO) + sizeof (*Flash);
  if ((Address == NULL) || (((UINTN)Address & 7) != 0) ||
      (Length < Required) || (Length > MAX_UINTN - (UINTN)Address))
  {
    return FALSE;
  }

  Info = Address;
  // This is deliberately the one-CPU CIX contract, not arbitrary TF-A input.
  if ((Info->Header.Type != 7) || (Info->Header.Version != 1) ||
      (Info->Header.Size != sizeof (*Info)) || (Info->Header.Attr != 0) ||
      (Info->NumCpus != 1) || (Info->NumSpMemRegions != 6) ||
      (Info->CpuInfo != (CONST EFI_SECURE_PARTITION_CPU_INFO *)(Info + 1)) ||
      (Info->SpSharedBufBase != (UINTN)Address) ||
      (Length > Info->SpSharedBufSize) ||
      (Info->SpImageBase != Info->SpMemBase) ||
      !CixLegacyRegion (Info->SpImageBase, Info->SpImageSize, Info->SpHeapBase) ||
      !CixLegacyRegion (Info->SpHeapBase, Info->SpHeapSize, Info->SpStackBase) ||
      !CixLegacyRegion (Info->SpStackBase, Info->SpPcpuStackSize, Info->SpSharedBufBase) ||
      !CixLegacyRegion (Info->SpSharedBufBase, Info->SpSharedBufSize, Info->SpMemLimit) ||
      (Info->SpNsCommBufSize == 0) || ((Info->SpNsCommBufBase & 4095) != 0) ||
      ((Info->SpNsCommBufSize & 4095) != 0) ||
      (Info->SpNsCommBufSize > MAX_UINT64 - Info->SpNsCommBufBase) ||
      ((Info->SpNsCommBufBase < Info->SpMemLimit) &&
       (Info->SpNsCommBufBase + Info->SpNsCommBufSize > Info->SpMemBase)))
  {
    return FALSE;
  }

  if ((Info->CpuInfo->LinearId != 0) || (Info->CpuInfo->Flags != CPU_INFO_FLAG_PRIMARY_CPU)) {
    return FALSE;
  }

  Flash = CixLegacyFlashInfo (Info);
  for (Index = 0; Index < 3; Index++) {
    if ((Flash->Size[Index] == 0) || ((Flash->Size[Index] & 4095) != 0) ||
        ((Flash->Base[Index] & 4095) != 0) ||
        (Flash->Base[Index] > 0x800000) ||
        (Flash->Size[Index] > 0x800000 - Flash->Base[Index]))
    {
      return FALSE;
    }
  }

  return (Flash->Base[1] == Flash->Base[0] + Flash->Size[0]) &&
         (Flash->Base[2] == Flash->Base[1] + Flash->Size[1]) &&
         (Flash->Size[0] == Flash->Size[2]);
}

#endif
