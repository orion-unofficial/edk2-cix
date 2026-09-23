/** @file
  Translate the CIX update protocol's image-type/status word to EFI_STATUS.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef CIX_FIRMWARE_STATUS_H_
#define CIX_FIRMWARE_STATUS_H_

#include <Uefi.h>
#include <Protocol/CixFwUpdateProtocol.h>

STATIC
EFI_STATUS
CixFirmwareResultStatus (
  UINT16  Result
  )
{
  // The upper byte identifies the image. EFI_ERROR cannot test this word.
  switch (Result & 0xff) {
    case FIRMWARE_RET_SUCCESS:
      return EFI_SUCCESS;
    case FIRMWARE_RET_ERR_INPUT:
      return EFI_INVALID_PARAMETER;
    case FIRMWARE_RET_OUT_OF_RESOURCE:
      return EFI_OUT_OF_RESOURCES;
    case FIRMWARE_RET_ERR_SIG:
      return EFI_SECURITY_VIOLATION;
    case FIRMWARE_RET_TYPE_NOT_SUPPORT:
    case FIRMWARE_RET_FUNC_NOT_FOUND:
      return EFI_UNSUPPORTED;
    default:
      return EFI_DEVICE_ERROR;
  }
}

#endif
