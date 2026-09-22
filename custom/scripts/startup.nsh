#!/usr/bin/env -S echo "This script is for UEFI Shell only:"

@echo -off

set product .

echo "************************************************************************"
echo "                       Radxa BIOS Update Utility"
echo "************************************************************************"
echo " "
echo "You are about to update the BIOS%product%"
echo "Please make sure the power stays on during the operation."
echo " "
echo "We strongly recommend to perform this operation locally, as the updated"
echo "system should be power cycled to ensure new code to be loaded."
echo "If you are performing this operation remotely, please make sure you have"
echo "means to also power cycle the system in case of issue."
echo " "
echo "If you decide to cancel BIOS update, you can run following commands:"
echo "    reset     to reboot the system"
echo "    reset -s  to shutdown the system"
echo " "
pause

echo "************************************************************************"
echo "                            Updating BIOS..."
echo "************************************************************************"
echo " "

if not exist "%0\..\FlashUpdate.efi" then
  echo "FlashUpdate.efi is missing; BIOS update was not started."
  exit /b 14
endif
if not exist "%0\..\cix_flash_all.bin" then
  echo "cix_flash_all.bin is missing; BIOS update was not started."
  exit /b 14
endif

"%0\..\FlashUpdate.efi" -f "%0\..\cix_flash_all.bin" -n
set -v edk2_cix_flash_status %lasterror%
if not %edk2_cix_flash_status% == 0 then
  echo "FlashUpdate.efi failed with status %edk2_cix_flash_status%."
  echo "BIOS update did not report success. Automatic shutdown cancelled."
  exit /b %edk2_cix_flash_status%
endif

echo " "
echo "************************************************************************"
echo "                         FlashUpdate.efi returned success."
echo "************************************************************************"
echo "This script does not independently verify flash readback."
echo "System will now power off."
echo "You MUST fully remove all connected power source before connecting them."
echo "Failure to do so may prevent some components to use the updated code."
echo " "
pause

reset -s "BIOS Update"

:EOF
