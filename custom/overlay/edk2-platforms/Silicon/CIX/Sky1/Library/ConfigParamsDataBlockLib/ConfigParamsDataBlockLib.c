// Keep the release-specific configuration data unchanged.
#define ParseConfigDataOption VendorParseConfigDataOption
#include "../../../../../../../../src/edk2-platforms/Silicon/CIX/Sky1/Library/ConfigParamsDataBlockLib/ConfigParamsDataBlockLib.c"
#undef ParseConfigDataOption

EFI_STATUS
EFIAPI
ParseConfigDataOption (
  IN  CHAR16                      *String,
  OUT UINT32                      *Num,
  OUT CONFIG_PARAMS_DATA_OPTIONS  *Options
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  CHAR16      *Head, *Tail;
  CHAR16      *Str;
  UINT32      Index;
  UINTN       Location, Size;

  if ((String == NULL) || (Num == NULL) || (Options == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Str = AllocateCopyPool (StrSize (String), String);
  if (Str == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Head = Str;
  Tail = Str;
  Size = StrSize (Str);

  Index    = 0;
  Location = 0;
  while ((Index < MAX_PARAMS_OPTION_NUM) && (Head != NULL) && (Location < Size)) {
    Head = StrStr (Tail, L":");
    if (Head == NULL) {
      FreePool (Str);
      return EFI_UNSUPPORTED;
    }

    *Head     = L'\0';
    Location += StrSize (Tail);
    if ((StrStr (Tail, L"0x") == NULL) && (StrStr (Tail, L"0X") == NULL)) {
      Options[Index].Value = StrDecimalToUint64 (Tail);
    } else {
      Options[Index].Value = StrHexToUint64 (Tail);
    }

    Tail = Head + 1;
    Head = StrStr (Tail, L",");
    if (Head == NULL) {
      Location += StrSize (Tail);
      if (Location != Size) {
        FreePool (Str);
        return EFI_UNSUPPORTED;
      }
    } else {
      *Head     = L'\0';
      Location += StrSize (Tail);
    }

    Status = StrCpyS (Options[Index].String, MAX_PARAMS_OPTION_STRING_SIZE, Tail);
    if (EFI_ERROR (Status)) {
      FreePool (Str);
      return Status;
    }

    if (Head != NULL) {
      Tail = Head + 1;
    }

    Index++;
  }

  FreePool (Str);
  *Num = Index;

  if ((Index == MAX_PARAMS_OPTION_NUM) && (Location < Size)) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}
