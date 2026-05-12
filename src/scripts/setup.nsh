#!/usr/bin/env -S echo "This script is for UEFI Shell only:"

@echo -off

echo "************************************************************************"
echo "                       Radxa BIOS Update Utility"
echo "************************************************************************"
echo " "
echo "You are about to update the BIOS."
echo "Please make sure the power stays on during the operation."
echo " "
pause

echo "************************************************************************"
echo "                            Updating BIOS..."
echo "************************************************************************"
echo " "

"%0\..\FlashUpdate.efi" -f "%0\..\cix_flash_all.bin" -n

echo " "
echo "************************************************************************"
echo "                         BIOS Update completed!"
echo "************************************************************************"
echo "System will now power off."
echo "You MUST fully remove all connected power source before connecting them."
echo "Failure to do so may prevent some components to use the updated code."
echo " "
pause

reset -s "BIOS Update"

:EOF
