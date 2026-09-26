[Defines]
  #
  # Custom path: keep firmware DEBUG() output on UART2 by default unless the
  # caller explicitly opts into consuming the UART3 header pins.
  #
!ifdef $(DEBUG_ON_UART3)
  DEFINE DEBUG_ON_UART3             = $(DEBUG_ON_UART3)
!else
  DEFINE DEBUG_ON_UART3             = FALSE
!endif
!ifdef $(UART3_ENABLE)
  DEFINE UART3_ENABLE               = $(UART3_ENABLE)
!else
  DEFINE UART3_ENABLE               = FALSE
!endif
!ifdef $(DEBUG_VERBOSE)
  DEFINE DEBUG_VERBOSE              = $(DEBUG_VERBOSE)
!else
  DEFINE DEBUG_VERBOSE              = FALSE
!endif
!ifdef $(DEBUG_PRINT_ERROR_LEVEL)
  DEFINE DEBUG_PRINT_ERROR_LEVEL    = $(DEBUG_PRINT_ERROR_LEVEL)
!else
  DEFINE DEBUG_PRINT_ERROR_LEVEL    = 0x80000040
!endif
!ifdef $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)
  DEFINE ENABLE_EXPERIMENTAL_UEFI_SETTINGS = $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS)
!else
  DEFINE ENABLE_EXPERIMENTAL_UEFI_SETTINGS = FALSE
!endif
!if $(ENABLE_EXPERIMENTAL_UEFI_SETTINGS) == TRUE
  DEFINE EXPERIMENTAL_UEFI_SETTINGS_UI_ENABLE = TRUE
  !if $(PLATFORM_NAME) == O6
    DEFINE EXPERIMENTAL_UEFI_SETTINGS_O6_ONLY_ENABLE = TRUE
  !else
    DEFINE EXPERIMENTAL_UEFI_SETTINGS_O6_ONLY_ENABLE = FALSE
  !endif
!else
  DEFINE EXPERIMENTAL_UEFI_SETTINGS_UI_ENABLE = FALSE
  DEFINE EXPERIMENTAL_UEFI_SETTINGS_O6_ONLY_ENABLE = FALSE
!endif
  #
  # Custom path: replace the in-tree CIX Secure Boot defaults with the public
  # Microsoft-managed defaults so Microsoft-signed Windows and shim-based Linux
  # media can boot without manual key enrollment.
  #
  DEFINE PK_DEFAULT_FILE            = Platform/Radxa/Platforms/CIX/Sky1/SecureBootDefaults/Microsoft/PK.bin
  DEFINE KEK_DEFAULT_FILE1          = Platform/Radxa/Platforms/CIX/Sky1/SecureBootDefaults/Microsoft/KEK.bin
  DEFINE DB_DEFAULT_FILE1           = Platform/Radxa/Platforms/CIX/Sky1/SecureBootDefaults/Microsoft/DB.bin
  DEFINE DBX_DEFAULT_FILE1          = Platform/Radxa/Platforms/CIX/Sky1/SecureBootDefaults/Microsoft/DBX.bin
  DEFINE TOKEN_SETUP_SUPPORT        = FALSE
  DEFINE SMBIOS_ENABLE              = FALSE
  DEFINE BOOT_LOGO_ENABLE           = FALSE
  DEFINE REALTEK_LAN_DRIVER_SUPPORT = FALSE

  # Cannot be lower than LOWEST_SUPPORTED_FIRMWARE_VERSION
  DEFINE BUILD_NUMBER               = 0x00000003
  DEFINE SYSTEM_FMP_ESRT_GUID       = 351c9808-4b53-4363-9314-d339386e5685
