/** @file
  Bind source storage drivers to the CIX NOR driver's FVB RAM shadow.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <PiMm.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Protocol/SmmFirmwareVolumeBlock.h>

STATIC CONST EFI_GUID  mNvramGuid = {
  0x4c8167fc, 0x5a9d, 0x468e, { 0xa9, 0x42, 0x86, 0x1f, 0xb5, 0x0a, 0xb5, 0x39 }
};

EFI_STATUS
EFIAPI
CixVariablePcdInit (
  VOID
  )
{
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *Fvb;
  EFI_PHYSICAL_ADDRESS               Base;
  EFI_STATUS                         Status;
  VOID                               *Hob;
  UINT64                             FlashBase[3];
  UINT32                             Size[3];

  // The CIX payload is packed: three UINT64 bases followed by three UINT32 sizes.
  Hob = GetFirstGuidHob (&mNvramGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) < 36)) {
    return EFI_NOT_FOUND;
  }

  CopyMem (FlashBase, GET_GUID_HOB_DATA (Hob), sizeof (FlashBase));
  CopyMem (Size, (UINT8 *)GET_GUID_HOB_DATA (Hob) + 24, sizeof (Size));
  // Validate each bound before subtracting the next base, and require a
  // contiguous variable/working/spare layout within the 8 MiB CIX NOR device.
  if ((Size[0] == 0) || (Size[1] == 0) || (Size[2] == 0) ||
      (Size[0] != Size[2]) ||
      (FlashBase[0] > 0x800000) || (Size[0] > 0x800000 - FlashBase[0]) ||
      (FlashBase[1] != FlashBase[0] + Size[0]) || (Size[1] > 0x800000 - FlashBase[1]) ||
      (FlashBase[2] != FlashBase[1] + Size[1]) || (Size[2] > 0x800000 - FlashBase[2]))
  {
    return EFI_COMPROMISED_DATA;
  }

  // The library DEPEX guarantees FVB exists before the constructor runs.
  // Use its standard accessor; no private vendor instance offsets are needed.
  Status = gMmst->MmLocateProtocol (
                    &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                    NULL,
                    (VOID **)&Fvb
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Fvb->GetPhysicalAddress (Fvb, &Base);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Base > MAX_UINT64 - Size[0] - Size[1] - Size[2]) {
    return EFI_COMPROMISED_DATA;
  }

  PatchPcdSet32 (PcdFlashNvStorageVariableSize, Size[0]);
  PatchPcdSet32 (PcdFlashNvStorageFtwWorkingSize, Size[1]);
  PatchPcdSet32 (PcdFlashNvStorageFtwSpareSize, Size[2]);
  PatchPcdSet64 (PcdFlashNvStorageVariableBase64, Base);
  PatchPcdSet64 (PcdFlashNvStorageFtwWorkingBase64, Base + Size[0]);
  PatchPcdSet64 (PcdFlashNvStorageFtwSpareBase64, Base + Size[0] + Size[1]);
  return EFI_SUCCESS;
}
