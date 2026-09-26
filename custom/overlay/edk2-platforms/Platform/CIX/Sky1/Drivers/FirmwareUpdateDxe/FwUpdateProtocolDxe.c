/** @CixFwUpdateProtocolDxe.c

  Copyright 2024 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "FirmwareUpdate.h"

// CIX_FWUP_PRIVATE_DATA    CixFwUpPrivateData;
CIX_FWUP_PRIVATE_DATA      *pFwPrivateData       = NULL;
EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  FimwareUpdateCallBack = NULL;
BOOLEAN                    IsOtaPackage          = FALSE;
BOOLEAN                    NeedFullPackageUpdate = TRUE;
EFI_DISK_IO_PROTOCOL       *NorFlashDiskIo       = NULL;
UINT32                     MediaId;
UINT32                     FlashBlockSize = SIZE_4KB;
UINTN                      FlashCapacity;
FIRMWARE_PROGRAM_STATUS    FwProgStatus;
UINT16                     GlobalPercentage;
EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  ProgressFunc = NULL;
// CHAR8   *pFirmwareHeader;

#define EFI_DEADLOOP()                  \
  {                                     \
    volatile UINTN __DeadLoopVar__ = 1; \
    while (__DeadLoopVar__)             \
      ;                                 \
  }

// void
// DoCallBack (
//   FIRMWARE_PROGRAM_STATUS  *pFwProgSts
//   )
// {
//   UINT32  Size = 0x2;

//   if ( FimwareUpdateCallBack != NULL) {
//     FimwareUpdateCallBack ((UINT8 *)pFwProgSts, Size);
//   }
// }

// These checks validate lengths and layout, not cryptographic authenticity.
STATIC
BOOLEAN
FlashRangeValid (
  UINT32  Offset,
  UINTN   Length
  )
{
  return (Offset <= FlashCapacity) && (Length <= FlashCapacity - Offset);
}

STATIC
VOID
FreeFirmwareUpdateState (
  VOID
  )
{
  if (pFwPrivateData != NULL) {
    if (pFwPrivateData->pNewImageFwHeader != NULL) {
      FreePool (pFwPrivateData->pNewImageFwHeader);
    }
    if (pFwPrivateData->pOnboardFwHeader != NULL) {
      FreePool (pFwPrivateData->pOnboardFwHeader);
    }
    FreePool (pFwPrivateData);
    pFwPrivateData = NULL;
  }
  IsOtaPackage = FALSE;
}

STATIC
UINT16
ValidateFirmwareHeader (
  FIRMWARE_HEADER  *Header,
  UINTN            Available,
  UINT32           *HeaderSize
  )
{
  UINT32          Index;
  UINT32          Other;
  FIRMWARE_ENTRY  *Entry;
  FIRMWARE_ENTRY  *Previous;

  if ((Header == NULL) || (Available < OFFSET_OF (FIRMWARE_HEADER, EntryNode)) ||
      (Header->Signature != FIRMWARE_HEADER_SIGNATURE) || (Header->EntryCount == 0) ||
      (Header->EntryCount > ARRAY_SIZE (((CIX_FWUP_PRIVATE_DATA *)0)->ImageFwEntryInfo))) {
    return FIRMWARE_RET_ERR_HEADER;
  }
  *HeaderSize = OFFSET_OF (FIRMWARE_HEADER, EntryNode) + Header->EntryCount * sizeof (FIRMWARE_ENTRY);
  if (*HeaderSize > Available) {
    return FIRMWARE_RET_ERR_HEADER;
  }
  for (Index = 0; Index < Header->EntryCount; Index++) {
    Entry = &Header->EntryNode[Index];
    if ((Entry->Type == 0) || (Entry->Type > MAX_UINT8) ||
        !FlashRangeValid (Entry->Base, Entry->Length)) {
      return FIRMWARE_RET_ERR_SIZE;
    }
    if ((FlashBlockSize == 0) || ((Entry->Base % FlashBlockSize) != 0)) {
      return FIRMWARE_RET_ERR_4KB_ALIGN;
    }
    for (Other = 0; Other < Index; Other++) {
      Previous = &Header->EntryNode[Other];
      if ((Entry->Type == Previous->Type) ||
          ((Entry->Length != 0) && (Previous->Length != 0) &&
           ((UINT64)Entry->Base < (UINT64)Previous->Base + Previous->Length) &&
           ((UINT64)Previous->Base < (UINT64)Entry->Base + Entry->Length))) {
        return FIRMWARE_RET_ERR_LAYOUT;
      }
    }
  }
  return FIRMWARE_RET_SUCCESS;
}

STATIC
UINT16
ReadOnboardHeader (
  FIRMWARE_HEADER  **Header,
  UINT32           *HeaderOffset
  )
{
  FIRMWARE_HEADER  *Buffer;
  UINT32           HeaderSize;
  UINT16           Result;
  EFI_STATUS       Status;

  *Header = NULL;
  Buffer = AllocateZeroPool (FlashBlockSize);
  if (Buffer == NULL) {
    return FIRMWARE_RET_OUT_OF_RESOURCE;
  }
  *HeaderOffset = FIRMWARE_HEADER_OFFSET;
  Status = CixFlashReadWrapper (*HeaderOffset, FlashBlockSize, Buffer);
  if (!EFI_ERROR (Status) && (Buffer->Signature != FIRMWARE_HEADER_SIGNATURE)) {
    *HeaderOffset = FIRMWARE_HEADER_OFFSET_ALT;
    Status = CixFlashReadWrapper (*HeaderOffset, FlashBlockSize, Buffer);
  }
  Result = EFI_ERROR (Status) ? FIRMWARE_RET_READ_ERR :
           ValidateFirmwareHeader (Buffer, FlashBlockSize, &HeaderSize);
  if (Result == FIRMWARE_RET_SUCCESS) {
    *Header = AllocateRuntimePool (HeaderSize);
    if (*Header == NULL) {
      Result = FIRMWARE_RET_OUT_OF_RESOURCE;
    } else {
      CopyMem (*Header, Buffer, HeaderSize);
    }
  }
  FreePool (Buffer);
  return Result;
}

UINT16
CixFirmwareUpdateInitialize (
  UINT8   *pNewFirmwareImage,
  UINT32  ImageSize,
  UINT32  dwflag
  )
{
  FIRMWARE_HEADER         *Header;
  FIRMWARE_ENTRY          *Entry;
  CIX_FIRMWARE_ENTRY_INFO *Info;
  UINT32                  Offset;
  UINT32                  HeaderSize;
  UINT32                  ImageOffset;
  UINT32                  Index;
  UINT16                  Result;
  EFI_STATUS              Status;

  FreeFirmwareUpdateState ();
  if ((pNewFirmwareImage == NULL) || (ImageSize < sizeof (FIRMWARE_HEADER))) {
    return FIRMWARE_RET_ERR_INPUT;
  }
  Status = LocateNorFlashDiskIoProtocol ();
  if (EFI_ERROR (Status)) {
    return FIRMWARE_RET_NO_FLASH_PROG_PROTOCOL;
  }
  Header = (FIRMWARE_HEADER *)pNewFirmwareImage;
  IsOtaPackage = (Header->Signature == FIRMWARE_HEADER_SIGNATURE) &&
                 ((Header->ControlFlag & UPDATE_OTA_PACKAGE) != 0);
  Offset = 0;
  if (!IsOtaPackage) {
    if (!FlashRangeValid (0, ImageSize)) {
      return FIRMWARE_RET_ERR_SIZE;
    }
    Offset = FIRMWARE_HEADER_OFFSET;
    if ((ImageSize < Offset + sizeof (FIRMWARE_HEADER)) ||
        (((FIRMWARE_HEADER *)(pNewFirmwareImage + Offset))->Signature != FIRMWARE_HEADER_SIGNATURE)) {
      Offset = FIRMWARE_HEADER_OFFSET_ALT;
    }
    if (ImageSize < Offset + sizeof (FIRMWARE_HEADER)) {
      return FIRMWARE_RET_ERR_HEADER;
    }
    Header = (FIRMWARE_HEADER *)(pNewFirmwareImage + Offset);
  }
  Result = ValidateFirmwareHeader (Header, ImageSize - Offset, &HeaderSize);
  if (Result != FIRMWARE_RET_SUCCESS) {
    return Result;
  }
  pFwPrivateData = AllocateRuntimeZeroPool (sizeof (*pFwPrivateData));
  if (pFwPrivateData == NULL) {
    return FIRMWARE_RET_OUT_OF_RESOURCE;
  }
  pFwPrivateData->pNewImageFwHeader = AllocateRuntimePool (HeaderSize);
  if (pFwPrivateData->pNewImageFwHeader == NULL) {
    Result = FIRMWARE_RET_OUT_OF_RESOURCE;
    goto Error;
  }
  CopyMem (pFwPrivateData->pNewImageFwHeader, Header, HeaderSize);
  pFwPrivateData->EntryCount = Header->EntryCount;
  pFwPrivateData->ImageBlockNum = ImageSize / FlashBlockSize;
  ImageOffset = HeaderSize;
  for (Index = 0; Index < Header->EntryCount; Index++) {
    Entry = &Header->EntryNode[Index];
    Info = &pFwPrivateData->ImageFwEntryInfo[Index];
    if (!IsOtaPackage) {
      ImageOffset = Entry->Base;
    }
    if ((ImageOffset > ImageSize) || (Entry->Length > ImageSize - ImageOffset)) {
      Result = FIRMWARE_RET_ERR_SIZE;
      goto Error;
    }
    Info->Type = Entry->Type;
    Info->EntryImageOffset = ImageOffset;
    Info->EntryImageSize = Entry->Length;
    ImageOffset += Entry->Length;
  }
  Result = ReadOnboardHeader (&pFwPrivateData->pOnboardFwHeader, &pFwPrivateData->HeaderOffset);
  if (Result == FIRMWARE_RET_SUCCESS) {
    return Result;
  }
Error:
  FreeFirmwareUpdateState ();
  return Result;
}


UINT16
CixFirmwareUpdateOnboardHeaderParse (
  VOID
  )
{
  UINT16          Result;
  UINT32          Index;
  FIRMWARE_ENTRY  *Entry;
  EFI_STATUS      Status;

  FreeFirmwareUpdateState ();
  Status = LocateNorFlashDiskIoProtocol ();
  if (EFI_ERROR (Status)) {
    return FIRMWARE_RET_NO_FLASH_PROG_PROTOCOL;
  }
  pFwPrivateData = AllocateRuntimeZeroPool (sizeof (*pFwPrivateData));
  if (pFwPrivateData == NULL) {
    return FIRMWARE_RET_OUT_OF_RESOURCE;
  }
  Result = ReadOnboardHeader (&pFwPrivateData->pOnboardFwHeader, &pFwPrivateData->HeaderOffset);
  if (Result != FIRMWARE_RET_SUCCESS) {
    FreeFirmwareUpdateState ();
    return Result;
  }
  pFwPrivateData->EntryCount = pFwPrivateData->pOnboardFwHeader->EntryCount;
  for (Index = 0; Index < pFwPrivateData->EntryCount; Index++) {
    Entry = &pFwPrivateData->pOnboardFwHeader->EntryNode[Index];
    pFwPrivateData->ImageFwEntryInfo[Index].Type = Entry->Type;
    pFwPrivateData->ImageFwEntryInfo[Index].EntryImageOffset = Entry->Base;
    pFwPrivateData->ImageFwEntryInfo[Index].EntryImageSize = Entry->Length;
  }
  return FIRMWARE_RET_SUCCESS;
}


EFI_STATUS
EFIAPI
CixFirmwareGetInfo (
  UINT8   *pNewFirmwareImage,
  UINT32  dwflag
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  // need check later
  return Status;
}

void
CixFirmwareProgramProgress (
  UINT32  BlockNum
  )
{
  UINT32  PercentData;

  PercentData = (BlockNum - pFwPrivateData->ProgramedBlock) * 100 / pFwPrivateData->ImageBlockNum;
  if (PercentData == 1) {
    pFwPrivateData->ProgramedBlock = BlockNum;
    PercentData                    = pFwPrivateData->ProgramedBlock * 100 / pFwPrivateData->ImageBlockNum;
    DEBUG ((DEBUG_INFO, "[FwU] Programming ... %02d %%\r", PercentData));
  }
}

BOOLEAN
IfNeedToUpdate (
  UINT8   Type,
  UINT8   *pImageBuff,
  UINT32  ImageBuffSize,
  UINT32  EntryOnboardAddress
  )
{
  CBFF_IMAGE_HEADER  Header;
  UINT32            Size;
  EFI_STATUS        Status;

  if ((Type == FIRMWARE_TYPE_MEM_CONF) || (Type == FIRMWARE_TYPE_PM_CONF) ||
      (Type == FIRMWARE_TYPE_BootLoader_3)) {
    return TRUE;
  }
  if ((Type != FIRMWARE_TYPE_BootLoader_1) && (Type != FIRMWARE_TYPE_BootLoader_2)) {
    return FALSE;
  }
  Size = (Type == FIRMWARE_TYPE_BootLoader_1) ? sizeof (CBFF_IMAGE_HEADER) : sizeof (FIP_TOC_HEADER);
  if ((pImageBuff == NULL) || (ImageBuffSize < Size)) {
    return FALSE;
  }
  Status = CixFlashReadWrapper (EntryOnboardAddress, Size, &Header);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  if (Type == FIRMWARE_TYPE_BootLoader_1) {
    return ((CBFF_IMAGE_HEADER *)pImageBuff)->Cbff_version >= Header.Cbff_version;
  }
  return ((FIP_TOC_HEADER *)pImageBuff)->serial_number >= ((FIP_TOC_HEADER *)&Header)->serial_number;
}


UINT16
CixFirmwareEntryUpdateWrapper (
  UINT8           Type,
  UINT8           *pNewFirmwareImage,
  FIRMWARE_ENTRY  *pEntryUpdate,
  FIRMWARE_ENTRY  *pEntryOnboard
  )
{
  UINT32                   Index;
  CIX_FIRMWARE_ENTRY_INFO  *Info;
  UINT8                    *Buffer;
  UINT16                   Result;

  Result = (UINT16)Type << 8;
  if ((pEntryUpdate->Base != pEntryOnboard->Base) ||
      (pEntryUpdate->Length != pEntryOnboard->Length)) {
    return Result | FIRMWARE_RET_ERR_LAYOUT;
  }
  for (Index = 0; Index < pFwPrivateData->EntryCount; Index++) {
    Info = &pFwPrivateData->ImageFwEntryInfo[Index];
    if (Info->Type != Type) {
      continue;
    }
    Buffer = pNewFirmwareImage + Info->EntryImageOffset;
    if (!IfNeedToUpdate (Type, Buffer, Info->EntryImageSize, pEntryOnboard->Base)) {
      return Result | FIRMWARE_RET_ERR_VER_DOWN;
    }
    return Result | CixFlashWriteWrapper (Type, Buffer, Info->EntryImageSize, 0, pEntryOnboard->Base);
  }
  return Result | FIRMWARE_RET_TYPE_NOT_FOUND;
}



UINT8
SplitAndWriteFirmware (
  UINT8   *pNewFirmwareImage,
  UINT32  ImageSize
  )
{
  UINT32  Offset;
  UINT32  Length;
  UINT8   Result;

  if ((pNewFirmwareImage == NULL) || (ImageSize == 0) || !FlashRangeValid (0, ImageSize)) {
    return FIRMWARE_RET_ERR_INPUT;
  }
  GlobalPercentage = 0;
  for (Offset = 0; Offset < ImageSize; Offset += Length) {
    Length = MIN (128 * 1024, ImageSize - Offset);
    Result = CixFlashWriteWrapper (FIRMWARE_TYPE_FULL_PROGRAM, pNewFirmwareImage + Offset, Length, 0, Offset);
    if (Result != FIRMWARE_RET_SUCCESS) {
      return Result;
    }
    GlobalPercentage = (UINT16)(((UINT64)Offset + Length) * 100 / ImageSize);
    if (ProgressFunc != NULL) {
      // An interrupted full-chip write may be unbootable; report failure if the caller aborts.
      if (EFI_ERROR (ProgressFunc (GlobalPercentage))) {
        return FIRMWARE_RET_ERR_PROG;
      }
    }
  }
  return FIRMWARE_RET_SUCCESS;
}


UINT16
CixFirmwareFullUpdatewrapper (
  UINT8   *pNewFirmwareImage,
  UINT32  ImageSize,
  UINT32  dwflag
  )
{
  return SplitAndWriteFirmware (pNewFirmwareImage, ImageSize);
}


UINT16
CixFirmwarePackageProgram (
  UINT8                                       *pNewFirmwareImage,
  UINT32                                      ImageSize,
  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS CallBackFunc
  )
{
  UINT16          Result;
  UINT32          Index;
  UINT32          Other;
  FIRMWARE_ENTRY  *Entry;
  FIRMWARE_ENTRY  *Onboard[10];

  FimwareUpdateCallBack = CallBackFunc;
  ProgressFunc = CallBackFunc;
  Result = CixFirmwareUpdateInitialize (pNewFirmwareImage, ImageSize, 0);
  if (Result != FIRMWARE_RET_SUCCESS) {
    return Result;
  }
  if (!IsOtaPackage) {
    Result = CixFirmwareFullUpdatewrapper (pNewFirmwareImage, ImageSize, 0);
    FreeFirmwareUpdateState ();
    return Result;
  }
  for (Index = 0; Index < pFwPrivateData->EntryCount; Index++) {
    Entry = &pFwPrivateData->pNewImageFwHeader->EntryNode[Index];
    Onboard[Index] = NULL;
    for (Other = 0; Other < pFwPrivateData->pOnboardFwHeader->EntryCount; Other++) {
      if (Entry->Type == pFwPrivateData->pOnboardFwHeader->EntryNode[Other].Type) {
        Onboard[Index] = &pFwPrivateData->pOnboardFwHeader->EntryNode[Other];
        break;
      }
    }
    if ((Onboard[Index] == NULL) || (Entry->Base != Onboard[Index]->Base) ||
        (Entry->Length != Onboard[Index]->Length)) {
      Result = (UINT16)((Entry->Type << 8) | FIRMWARE_RET_ERR_LAYOUT);
      goto Done;
    }
  }
  for (Index = 0; Index < pFwPrivateData->EntryCount; Index++) {
    Entry = &pFwPrivateData->pNewImageFwHeader->EntryNode[Index];
    Result = CixFirmwareEntryUpdateWrapper (Entry->Type, pNewFirmwareImage, Entry, Onboard[Index]);
    if ((Result & 0xff) != FIRMWARE_RET_SUCCESS) {
      goto Done;
    }
  }
  Result = FIRMWARE_RET_SUCCESS;
Done:
  FreeFirmwareUpdateState ();
  return Result;
}


UINT16
CixFirmwareRead (
  UINT32  OffsetAddress,
  UINTN   ImageBuffSize,
  UINT8   *pBuff
  )
{
  EFI_STATUS  Status;

  Status = LocateNorFlashDiskIoProtocol ();
  if (EFI_ERROR (Status)) {
    return FIRMWARE_RET_NO_FLASH_PROG_PROTOCOL;
  }

  Status = CixFlashReadWrapper (OffsetAddress, ImageBuffSize, pBuff);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: status %r\n", __FUNCTION__, Status));
    return FIRMWARE_RET_READ_ERR;
  }

  return FIRMWARE_RET_SUCCESS;
}

UINT16
CixFirmwareReadNull (
  UINT32  OffsetAddress,
  UINTN   ImageBuffSize,
  UINT8   *pBuff
  )
{
  return FIRMWARE_RET_FUNC_NOT_FOUND;
}

EFI_STATUS
EFIAPI
CixFlashReadWrapper (
  UINT32  OffsetAddress,
  UINTN   ImageBuffSize,
  VOID    *pImageBuff
  )
{
  EFI_STATUS  Status;

  // CHAR8 *pBuffer;

  // The buffer must be valid
  if ((pImageBuff == NULL) || !FlashRangeValid (OffsetAddress, ImageBuffSize)) {
    return EFI_INVALID_PARAMETER;
  }

  // Return if we have no byte to read
  if (ImageBuffSize == 0) {
    return EFI_SUCCESS;
  }

  if (NorFlashDiskIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = NorFlashDiskIo->ReadDisk (NorFlashDiskIo, MediaId, OffsetAddress, ImageBuffSize, pImageBuff);
  // DebugPrint (DEBUG_INFO, "[FwU] Read NorFlash status : %r\n", Status);

  /*
  {
    UINT32 i;
    pBuffer = (CHAR8*)pImageBuff;
    for(i=0;i<256;i++,pBuffer++){

      DEBUG ((DEBUG_INFO, "%02x ",*pBuffer));
      if ( (i+1)%16 == 0 ){
       DEBUG ((DEBUG_INFO, " \n"));
      }

    }
  } */
  return Status;
}

BOOLEAN
IsNorFlashDevicePath (
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  // EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;
  VENDOR_DEVICE_PATH        *VendorDevicePath;

  // EFI_HANDLE                Handle;

  TempDevicePath = DevicePath;
  while (!IsDevicePathEnd (TempDevicePath)) {
    if ((DevicePathType (TempDevicePath) == HARDWARE_DEVICE_PATH) &&
        (DevicePathSubType (TempDevicePath) == HW_VENDOR_DP))
    {
      VendorDevicePath = (VENDOR_DEVICE_PATH *)TempDevicePath;
      if (CompareGuid (&(VendorDevicePath->Guid), &gCixNorFlashDevicePathGuid)) {
        DEBUG ((DEBUG_INFO, "[FwU] gNorFlashDevicePathGuid %g found.\n", &gCixNorFlashDevicePathGuid));
        return TRUE;
      }
    }

    TempDevicePath = NextDevicePathNode (TempDevicePath);
  }

  return FALSE;
}

EFI_STATUS
EFIAPI
LocateNorFlashDiskIoProtocol (
  )
{
  EFI_HANDLE                *DiskIoHandles;
  UINTN                     NumberDiskIoHandles;
  UINTN                     Index;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_STATUS                Status;
  NOR_FLASH_INSTANCE        *Instance;

  NorFlashDiskIo = NULL;
  FlashCapacity = 0;

  // find DiskIoProtocol
  Status = gBS->LocateHandleBuffer (
                                    ByProtocol,
                                    &gEfiDiskIoProtocolGuid,
                                    NULL,
                                    &NumberDiskIoHandles,
                                    &DiskIoHandles
                                    );

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  // DEBUG ((DEBUG_INFO, "[FwU] NumberDiskIoHandles %d\n", NumberDiskIoHandles));
  for (Index = 0; Index < NumberDiskIoHandles; Index++) {
    DevicePath = DevicePathFromHandle (DiskIoHandles[Index]);
    if (!DevicePath) {
      continue;
    }

    if (IsNorFlashDevicePath (DevicePath)) {
      MediaId = ((NOR_FLASH_DEVICE_PATH *)DevicePath)->Index;
      Status  = gBS->HandleProtocol (DiskIoHandles[Index], &gEfiDiskIoProtocolGuid, (VOID **)&NorFlashDiskIo);
      if (!EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "[FwU] locate NorFlash DiskIoProtocol success\n"));
        Instance       = INSTANCE_FROM_DISKIO_THIS (NorFlashDiskIo);
        FlashBlockSize = Instance->Media.BlockSize;
        // The vendor driver describes a 16 MiB mapping; O6/O6N have an
        // 8 MiB physical flash. Never let the mapped aperture widen writes.
        FlashCapacity  = MIN (Instance->Size, SIZE_8MB);
        MediaId        = Instance->Media.MediaId;
        FreePool (DiskIoHandles);
        if ((FlashBlockSize < sizeof (FIRMWARE_HEADER)) || (FlashCapacity < FlashBlockSize)) {
          NorFlashDiskIo = NULL;
          FlashCapacity = 0;
          return EFI_DEVICE_ERROR;
        }
        return EFI_SUCCESS;
      }
    }
  }

  FreePool (DiskIoHandles);
  NorFlashDiskIo = NULL;

  if (Index == NumberDiskIoHandles) {
    DEBUG ((DEBUG_INFO, "[FwU]Not found NorFlash device.\n"));
    return EFI_NOT_FOUND;
  }

  return Status;
}

BOOLEAN
CixFirmwareVerify (
  UINT8                    *pImageBuff,
  UINT32                   ImageBuffSize,
  UINT32                   OffsetAddress,
  FIRMWARE_PROGRAM_STATUS  *pFwProgSts
  )
{
  EFI_STATUS  Status;
  UINT8       *Buffer;
  BOOLEAN     Matches;

  if ((pImageBuff == NULL) || (ImageBuffSize == 0) || !FlashRangeValid (OffsetAddress, ImageBuffSize)) {
    return FALSE;
  }
  Buffer = AllocatePool (ImageBuffSize);
  if (Buffer == NULL) {
    return FALSE;
  }
  Status = CixFlashReadWrapper (OffsetAddress, ImageBuffSize, Buffer);
  Matches = !EFI_ERROR (Status) && (CompareMem (pImageBuff, Buffer, ImageBuffSize) == 0);
  FreePool (Buffer);
  return Matches;
}


UINT8
CixFlashWriteWrapper (
  UINT8   Type,
  UINT8   *pImageBuff,
  UINT32  ImageBuffSize,
  UINT32  dwflag,
  UINT32  OffsetAddress
  )
{
  EFI_STATUS  Status;
  BOOLEAN     bVerify = FALSE;

  if (NorFlashDiskIo == NULL) {
    return FIRMWARE_RET_NO_FLASH_PROG_PROTOCOL;
  }

  if ((pImageBuff == NULL) || (ImageBuffSize == 0) || !FlashRangeValid (OffsetAddress, ImageBuffSize)) {
    return FIRMWARE_RET_ERR_INPUT;
  }

  FwProgStatus.firmware_type = Type;
  FwProgStatus.status_result = FIRMWARE_START;
  //DoCallBack (&FwProgStatus);
  DEBUG ((DEBUG_INFO, "[FwU] Program entry %d, address 0x%08x\n", Type, OffsetAddress));
  Status = NorFlashDiskIo->WriteDisk (NorFlashDiskIo, MediaId, OffsetAddress, ImageBuffSize, pImageBuff);
  // DebugPrint (DEBUG_INFO, "[FwU] Write NorFlash status : %r\n", Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: status %r\n", __FUNCTION__, Status));
    FwProgStatus.status_result = FIRMWARE_WRITE_FAILED;
    //DoCallBack (&FwProgStatus);
    return FIRMWARE_RET_ERR_PROG;
  }

  FwProgStatus.status_result = FIRMWARE_WRITE_SUCCESS;
  //DoCallBack (&FwProgStatus);
  // do verify
  bVerify = CixFirmwareVerify (pImageBuff, ImageBuffSize, OffsetAddress, &FwProgStatus);
  if (!bVerify) {
    FwProgStatus.status_result = FIRMWARE_VERIFY_FAILED;
    //DoCallBack (&FwProgStatus);
    return FIRMWARE_RET_ERR_VERIFY;
  }

  FwProgStatus.status_result = FIRMWARE_PROGRAM_SUCCESS;
  //DoCallBack (&FwProgStatus);

  return FIRMWARE_RET_SUCCESS;
}

UINT16
CixFirmwareSingleProgram (
  UINT8                                       Type,
  UINT8                                       *pNewFirmwareImage,
  UINT32                                      ImageSize,
  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS CallBackFunc
  )
{
  UINT16          Result;
  UINT32          Index;
  FIRMWARE_ENTRY  *NewEntry = NULL;
  FIRMWARE_ENTRY  *OldEntry = NULL;

  if ((Type != FIRMWARE_TYPE_BootLoader_1) && (Type != FIRMWARE_TYPE_BootLoader_2) &&
      (Type != FIRMWARE_TYPE_MEM_CONF) && (Type != FIRMWARE_TYPE_PM_CONF) &&
      (Type != FIRMWARE_TYPE_BootLoader_3)) {
    return ((UINT16)Type << 8) | FIRMWARE_RET_TYPE_NOT_SUPPORT;
  }
  Result = CixFirmwareUpdateInitialize (pNewFirmwareImage, ImageSize, 0);
  if (Result != FIRMWARE_RET_SUCCESS) {
    return ((UINT16)Type << 8) | Result;
  }
  for (Index = 0; Index < pFwPrivateData->pNewImageFwHeader->EntryCount; Index++) {
    if (pFwPrivateData->pNewImageFwHeader->EntryNode[Index].Type == Type) {
      NewEntry = &pFwPrivateData->pNewImageFwHeader->EntryNode[Index];
    }
  }
  for (Index = 0; Index < pFwPrivateData->pOnboardFwHeader->EntryCount; Index++) {
    if (pFwPrivateData->pOnboardFwHeader->EntryNode[Index].Type == Type) {
      OldEntry = &pFwPrivateData->pOnboardFwHeader->EntryNode[Index];
    }
  }
  Result = ((UINT16)Type << 8) | FIRMWARE_RET_TYPE_NOT_FOUND;
  if ((NewEntry != NULL) && (OldEntry != NULL)) {
    Result = CixFirmwareEntryUpdateWrapper (Type, pNewFirmwareImage, NewEntry, OldEntry);
  }
  FreeFirmwareUpdateState ();
  return Result;
}

UINT16
CixFirmwareRawEntryUpdateNull (
  UINT8                      Type,
  UINT8                      *pEntryImage,
  UINT32                     ImageSize,
  ENTRY_UPDATE_METHOD        UpateState,
  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS  CallBackFunc
  )
{
  return FIRMWARE_RET_FUNC_NOT_FOUND;
}

// Type      : 3/4/5
// ImageSize : entry size
// UpateState: 1 - protram ; 0 - erase ; 2 - read
UINT16
CixFirmwareRawEntryUpdate (
  UINT8                                       Type,
  UINT8                                       *pEntryImage,
  UINT32                                      ImageSize,
  ENTRY_UPDATE_METHOD                         UpateState,
  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS CallBackFunc
  )
{
  UINT16          Result;
  UINT32          Index;
  UINT32          Address;
  UINT32          End;
  UINT8           *Buffer;
  FIRMWARE_ENTRY  *Entry;

  Result = (UINT16)Type << 8;
  if (((Type != FIRMWARE_TYPE_SECURE_DEBUG) && (Type != FIRMWARE_TYPE_MEM_CONF) &&
       (Type != FIRMWARE_TYPE_PM_CONF)) ||
      (ImageSize != SIZE_4KB) || ((UINT32)UpateState > ENTRY_READ) ||
      ((UpateState != ENTRY_ERASE) && (pEntryImage == NULL))) {
    return Result | FIRMWARE_RET_ERR_INPUT;
  }
  Result |= CixFirmwareUpdateOnboardHeaderParse ();
  if ((Result & 0xff) != FIRMWARE_RET_SUCCESS) {
    return Result;
  }
  Address = MAX_UINT32;
  for (Index = 0; Index < pFwPrivateData->EntryCount; Index++) {
    Entry = &pFwPrivateData->pOnboardFwHeader->EntryNode[Index];
    if (Entry->Type == Type) {
      Address = Entry->Base;
      break;
    }
  }
  if (Address == MAX_UINT32) {
    Result |= FIRMWARE_RET_TYPE_NOT_FOUND;
    goto Done;
  }
  if (!FlashRangeValid (Address, ImageSize)) {
    Result |= FIRMWARE_RET_ERR_SIZE;
    goto Done;
  }
  End = Address + ImageSize;
  if ((Address <= pFwPrivateData->HeaderOffset) && (End > pFwPrivateData->HeaderOffset)) {
    Result |= FIRMWARE_RET_ERR_LAYOUT;
    goto Done;
  }
  for (Index = 0; Index < pFwPrivateData->EntryCount; Index++) {
    Entry = &pFwPrivateData->pOnboardFwHeader->EntryNode[Index];
    if ((Entry->Type != Type) && (Entry->Base >= Address) && (Entry->Base < End)) {
      Result |= FIRMWARE_RET_ERR_LAYOUT;
      goto Done;
    }
  }
  if (UpateState == ENTRY_READ) {
    if (EFI_ERROR (CixFlashReadWrapper (Address, ImageSize, pEntryImage))) {
      Result |= FIRMWARE_RET_READ_ERR;
    }
  } else if (UpateState == ENTRY_WRITE) {
    Result |= CixFlashWriteWrapper (Type, pEntryImage, ImageSize, 0, Address);
  } else {
    Buffer = AllocatePool (ImageSize);
    if (Buffer == NULL) {
      Result |= FIRMWARE_RET_OUT_OF_RESOURCE;
      goto Done;
    }
    SetMem (Buffer, ImageSize, 0xff);
    Result |= CixFlashWriteWrapper (Type, Buffer, ImageSize, 0, Address);
    FreePool (Buffer);
  }
Done:
  FreeFirmwareUpdateState ();
  return Result;
}


UINT32
ReadFwVersionOnboard (
  UINT8  Type
  )
{
  FIRMWARE_HEADER  *pCixOnboardFwHeader;
  FIRMWARE_ENTRY   *pEntryOnboard;
  UINT32           Index, FwVersion = 0xFFFFFFFF;
  BOOLEAN          EntryFound = FALSE;

  VOID  *pBuff;

  pCixOnboardFwHeader = pFwPrivateData->pOnboardFwHeader;

  for (Index = 0; Index < pCixOnboardFwHeader->EntryCount; Index++) {
    if (pCixOnboardFwHeader->EntryNode[Index].Type == Type) {
      pEntryOnboard = &(pCixOnboardFwHeader->EntryNode[Index]);
      EntryFound    = TRUE;
      break;
    }
  }

  if (!EntryFound) {
    return FwVersion;
  }

  pBuff = AllocateZeroPool (FlashBlockSize);

  if (pBuff == NULL) {
    return FwVersion;
  }
  if (EFI_ERROR (CixFlashReadWrapper (pEntryOnboard->Base, FlashBlockSize, pBuff))) {
    FreePool (pBuff);
    return FwVersion;
  }
  switch (Type) {
    case FIRMWARE_TYPE_BootLoader_1:
      FwVersion = ((CBFF_IMAGE_HEADER *)pBuff)->Cbff_version;
      break;
    case FIRMWARE_TYPE_BootLoader_2:
    case FIRMWARE_TYPE_BootLoader_3:
      FwVersion = ((FIP_TOC_HEADER *)pBuff)->serial_number;
      break;
    case FIRMWARE_TYPE_MEM_CONF:
    case FIRMWARE_TYPE_PM_CONF:
    default:
      break;
  }

  FreePool (pBuff);
  DEBUG ((DEBUG_INFO, "[FwU]FwType:%d FwVersion:0x%08x\n", Type, FwVersion));
  return FwVersion;
}

UINT16
CixFirmwareGetVersion (
  UINT32  *HeaderOffset,
  UINT32  *Bootloader1Ver,
  UINT32  *Bootloader2Ver,
  UINT32  *Bootloader3Ver
  )
{
  UINT16  ReturnCode;

  if ((Bootloader1Ver == NULL) || (Bootloader2Ver == NULL) || (Bootloader3Ver == NULL) || (HeaderOffset == NULL)) {
    ReturnCode = FIRMWARE_RET_ERR_INPUT;
    return ReturnCode;
  }

  ReturnCode = CixFirmwareUpdateOnboardHeaderParse ();
  if (ReturnCode != FIRMWARE_RET_SUCCESS) {
    return ReturnCode;
  }

  *HeaderOffset   = pFwPrivateData->HeaderOffset;
  *Bootloader1Ver = ReadFwVersionOnboard (FIRMWARE_TYPE_BootLoader_1);
  *Bootloader2Ver = ReadFwVersionOnboard (FIRMWARE_TYPE_BootLoader_2);
  *Bootloader3Ver = ReadFwVersionOnboard (FIRMWARE_TYPE_BootLoader_3);
  FreeFirmwareUpdateState ();
  if ((*Bootloader1Ver == MAX_UINT32) || (*Bootloader2Ver == MAX_UINT32) || (*Bootloader3Ver == MAX_UINT32)) {
    return FIRMWARE_RET_READ_ERR;
  }
  return FIRMWARE_RET_SUCCESS;
}

UINT16
CixFirmwareGetPercentage (
  )
{
  return GlobalPercentage;
}

CIX_FW_UPDATE_PROTOCOL  CixFwUpdateFuncList = {
  FW_UPDATE_PROTOCOL_VERSION,
  CixFirmwarePackageProgram,
  CixFirmwareSingleProgram,
  CixFirmwareReadNull,
  CixFirmwareRawEntryUpdate,
  CixFirmwareGetVersion,
  CixFirmwareGetPercentage
};

EFI_STATUS
EFIAPI
CixFirmwareUpdateDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "[FwU]%a start.\n", __FUNCTION__));

 #ifdef DEBUG_MODE
  CixFwUpdateFuncList.FirmwareRead =  CixFirmwareRead;
  // CixFwUpdateFuncList.FirmwareRawEntryUpdate = CixFirmwareRawEntryUpdate;
 #endif

  Status = gBS->InstallProtocolInterface (
                                          &ImageHandle,
                                          &gCixFirmwareUpdateProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &CixFwUpdateFuncList
                                          );
  if (EFI_ERROR (Status)) {
    DEBUG (
           (DEBUG_ERROR,
            "[FwU]%a: failed to install FirmwareUpdate protocol (Status == %r)\n",
            __FUNCTION__, Status)
           );
    return Status;
  }

  // update ec firmware
  CixFirmwareEcProgram ();

  return EFI_SUCCESS;
}
