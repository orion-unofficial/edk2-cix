/** @file
 *
 *  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Uefi.h>
#include <IndustryStandard/Acpi.h>
#include <libfdt.h>

#include <Guid/ConsolePrefFormSet.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/AcpiTable.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/GraphicsOutput.h>

#define SPCR_SIG  EFI_ACPI_2_0_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE

#define CONSOLE_PREF_GRAPHICAL  0x0
#define CONSOLE_PREF_SERIAL     0x1

#define CONSOLE_PREF_VARIABLE_NAME  L"ConsolePref"

typedef struct {
  UINT8    Console;
  UINT8    Reserved[3];
} CONSOLE_PREF_VARSTORE_DATA;

STATIC EFI_EVENT  mReadyToBootEvent;

STATIC
VOID
RemoveDtStdoutPath (
  VOID
  )
{
  VOID        *Dtb;
  INT32       Node;
  INT32       Error;
  EFI_STATUS  Status;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Dtb);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: could not retrieve DT blob - %r\n", __FUNCTION__, Status));
    return;
  }

  Node = fdt_path_offset (Dtb, "/chosen");
  if (Node < 0) {
    return;
  }

  Error = fdt_delprop (Dtb, Node, "stdout-path");
  if (Error != 0) {
    DEBUG ((DEBUG_INFO, "%a: failed to delete 'stdout-path' property: %a\n", __FUNCTION__, fdt_strerror (Error)));
  }
}

STATIC
VOID
RemoveSpcrTable (
  VOID
  )
{
  EFI_ACPI_SDT_PROTOCOL    *Sdt;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTable;
  EFI_STATUS               Status;
  UINTN                    TableIndex;
  EFI_ACPI_SDT_HEADER      *TableHeader;
  EFI_ACPI_TABLE_VERSION   TableVersion;
  UINTN                    TableKey;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = gBS->LocateProtocol (&gEfiAcpiSdtProtocolGuid, NULL, (VOID **)&Sdt);
  if (EFI_ERROR (Status)) {
    return;
  }

  TableIndex  = 0;
  TableKey    = 0;
  TableHeader = NULL;

  do {
    Status = Sdt->GetAcpiTable (TableIndex++, &TableHeader, &TableVersion, &TableKey);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (TableHeader->Signature != SPCR_SIG) {
      continue;
    }

    Status = AcpiTable->UninstallAcpiTable (AcpiTable, TableKey);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: failed to uninstall SPCR table - %r\n", __FUNCTION__, Status));
    }

    break;
  } while (TRUE);
}

STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  CONSOLE_PREF_VARSTORE_DATA  ConsolePref;
  UINTN                       BufferSize;
  EFI_STATUS                  Status;
  VOID                        *Gop;

  BufferSize = sizeof (ConsolePref);
  Status     = gRT->GetVariable (
                      CONSOLE_PREF_VARIABLE_NAME,
                      &gConsolePrefFormSetGuid,
                      NULL,
                      &BufferSize,
                      &ConsolePref
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: variable '%s' could not be read - bailing!\n", __FUNCTION__, CONSOLE_PREF_VARIABLE_NAME));
    return;
  }

  if (ConsolePref.Console == CONSOLE_PREF_SERIAL) {
    DEBUG ((DEBUG_INFO, "%a: serial console handoff preserved - doing nothing\n", __FUNCTION__));
    return;
  }

  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, &Gop);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: no GOP instances found - doing nothing (%r)\n", __FUNCTION__, Status));
    return;
  }

  RemoveDtStdoutPath ();
  RemoveSpcrTable ();
}

EFI_STATUS
EFIAPI
ConsolePrefDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  CONSOLE_PREF_VARSTORE_DATA  ConsolePref;
  UINTN                       BufferSize;

  BufferSize = sizeof (ConsolePref);
  Status     = gRT->GetVariable (
                      CONSOLE_PREF_VARIABLE_NAME,
                      &gConsolePrefFormSetGuid,
                      NULL,
                      &BufferSize,
                      &ConsolePref
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: no console preference found, defaulting to serial handoff preserved\n", __FUNCTION__));
    ConsolePref.Console = CONSOLE_PREF_SERIAL;
  }

  if (!EFI_ERROR (Status) &&
      (ConsolePref.Console != CONSOLE_PREF_GRAPHICAL) &&
      (ConsolePref.Console != CONSOLE_PREF_SERIAL))
  {
    DEBUG ((DEBUG_WARN, "%a: invalid value for %s, defaulting to serial handoff preserved\n", __FUNCTION__, CONSOLE_PREF_VARIABLE_NAME));
    ConsolePref.Console = CONSOLE_PREF_SERIAL;
    Status              = EFI_INVALID_PARAMETER;
  }

  if (EFI_ERROR (Status)) {
    ZeroMem (&ConsolePref.Reserved, sizeof (ConsolePref.Reserved));
    Status = gRT->SetVariable (
                    CONSOLE_PREF_VARIABLE_NAME,
                    &gConsolePrefFormSetGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                    sizeof (ConsolePref),
                    &ConsolePref
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: gRT->SetVariable () failed - %r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnReadyToBoot,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &mReadyToBootEvent
                  );
  ASSERT_EFI_ERROR (Status);
  return Status;
}
