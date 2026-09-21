/** @file
  Read-only CIX reboot reason registers. The selected vendor boot chain determines
  the scratch value encoding; the displayed firmware version does not.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#if CIX_REBOOT_SCRATCH_LAYOUT != 1 && CIX_REBOOT_SCRATCH_LAYOUT != 2
#error "A verified vendor reboot scratch layout is required"
#endif
Device (RBRR) {
  Name (_HID, "PRP0001")
  Name (_UID, Zero)
  Name (_DSD, Package () {
    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
    Package () {
      Package () { "compatible", "cix,sky1-reboot-reason" },
      Package () { "cix,firmware-scratch-layout", CIX_REBOOT_SCRATCH_LAYOUT }
    }
  })
  Name (_CRS, ResourceTemplate () {
    Memory32Fixed (ReadOnly, 0x16000500, 0x04)
    Memory32Fixed (ReadOnly, 0x16000218, 0x04)
  })
}
