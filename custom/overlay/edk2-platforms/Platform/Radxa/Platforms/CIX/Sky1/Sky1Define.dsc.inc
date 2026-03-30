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
!ifdef $(DEBUG_PRINT_ERROR_LEVEL)
  DEFINE DEBUG_PRINT_ERROR_LEVEL    = $(DEBUG_PRINT_ERROR_LEVEL)
!else
  DEFINE DEBUG_PRINT_ERROR_LEVEL    = 0x80000040
!endif
  #
  # Custom path: replace the in-tree CIX Secure Boot defaults with the public
  # Microsoft-managed defaults so Microsoft-signed Windows and shim-based Linux
  # media can boot without manual key enrollment.
  #
  DEFINE PK_DEFAULT_FILE            = Platform/CIX/Sky1/Drivers/SecureBootDxe/Microsoft/PK.bin
  DEFINE KEK_DEFAULT_FILE1          = Platform/CIX/Sky1/Drivers/SecureBootDxe/Microsoft/KEK.bin
  DEFINE DB_DEFAULT_FILE1           = Platform/CIX/Sky1/Drivers/SecureBootDxe/Microsoft/DB.bin
  DEFINE DBX_DEFAULT_FILE1          = Platform/CIX/Sky1/Drivers/SecureBootDxe/Microsoft/DBX.bin
  DEFINE TOKEN_SETUP_SUPPORT        = FALSE
  DEFINE SMBIOS_ENABLE              = FALSE
  DEFINE BOOT_LOGO_ENABLE           = FALSE
  DEFINE REALTEK_LAN_DRIVER_SUPPORT = FALSE

  # Cannot be lower than LOWEST_SUPPORTED_FIRMWARE_VERSION
  DEFINE BUILD_NUMBER               = 0x00000003
  DEFINE SYSTEM_FMP_ESRT_GUID       = 351c9808-4b53-4363-9314-d339386e5685
